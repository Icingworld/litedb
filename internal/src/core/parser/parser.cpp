#include "core/parser/parser.hpp"

#include <charconv>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "core/parser/lexer.hpp"
#include "core/parser/ast/expression/between_expression.hpp"
#include "core/parser/ast/expression/binary_expression.hpp"
#include "core/parser/ast/expression/column_reference_expression.hpp"
#include "core/parser/ast/expression/expression_node.hpp"
#include "core/parser/ast/expression/in_expression.hpp"
#include "core/parser/ast/expression/like_expression.hpp"
#include "core/parser/ast/expression/literal_expression.hpp"
#include "core/parser/ast/expression/unary_expression.hpp"
#include "core/parser/ast/expression/vector_expression.hpp"
#include "core/parser/ast/expression/wildcard_expression.hpp"
#include "core/parser/ast/statement/create_collection_statement.hpp"
#include "core/parser/ast/statement/create_database_statement.hpp"
#include "core/parser/ast/statement/delete_statement.hpp"
#include "core/parser/ast/statement/describe_statement.hpp"
#include "core/parser/ast/statement/drop_statement.hpp"
#include "core/parser/ast/statement/insert_statement.hpp"
#include "core/parser/ast/statement/select_statement.hpp"
#include "core/parser/ast/statement/show_statement.hpp"
#include "core/parser/ast/statement/statement_node.hpp"
#include "core/parser/ast/statement/update_statement.hpp"
#include "core/parser/ast/statement/use_statement.hpp"

namespace litedb::core::parser
{

namespace
{

[[nodiscard]]
bool is_comparison_operator(TokenType type) noexcept
{
    return type == TokenType::Equal
        || type == TokenType::NotEqual
        || type == TokenType::LessThan
        || type == TokenType::LessEqual
        || type == TokenType::GreaterThan
        || type == TokenType::GreaterEqual;
}

[[nodiscard]]
bool is_literal_token(TokenType type) noexcept
{
    return type == TokenType::IntegerLiteral
        || type == TokenType::FloatLiteral
        || type == TokenType::StringLiteral
        || type == TokenType::True
        || type == TokenType::False
        || type == TokenType::Null;
}

} // namespace

Parser::Parser(std::string input)
    : lexer_(std::make_unique<Lexer>(std::move(input)))
    , current_token_(TokenType::EoF, "", TokenLocation {1, 1})
{
}

Parser::Parser(std::unique_ptr<Lexer> lexer)
    : lexer_(std::move(lexer))
    , current_token_(TokenType::EoF, "", TokenLocation {1, 1})
{
}

Parser::~Parser() = default;

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> Parser::parse()
{
    current_token_ = lexer_->next();

    if (current_token_.type() == TokenType::EoF) [[unlikely]] {
        return std::unexpected(ParserError {current_token_.location(), "Empty statement"});
    }

    auto statement = parse_statement();
    if (!statement.has_value()) [[unlikely]] {
        return std::unexpected(statement.error());
    }

    skip_semicolon();

    if (current_token_.type() != TokenType::EoF) [[unlikely]] {
        return std::unexpected(ParserError {current_token_.location(), "Unexpected token"});
    }

    return std::move(statement.value());
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> Parser::parse_statement()
{
    switch (current_token_.type()) {
    case TokenType::Use:
        return parse_use_statement();
    case TokenType::Create:
        return parse_create_statement();
    case TokenType::Drop:
        return parse_drop_statement();
    case TokenType::Show:
        return parse_show_statement();
    case TokenType::Describe:
    case TokenType::Desc:
        return parse_describe_statement();
    case TokenType::Insert:
        return parse_insert_statement();
    case TokenType::Update:
        return parse_update_statement();
    case TokenType::Delete:
        return parse_delete_statement();
    case TokenType::Select:
        return parse_select_statement();
    default:
        return std::unexpected(ParserError {current_token_.location(), "Unexpected statement"});
    }
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> Parser::parse_use_statement()
{
    const TokenLocation location = current_token_.location();
    (void) advance();

    auto database = parse_identifier_string("Expected database name");
    if (!database.has_value()) {
        return std::unexpected(database.error());
    }

    return std::make_unique<ast::UseStatement>(
        std::move(database.value()),
        ast_location(location)
    );
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> Parser::parse_create_statement()
{
    const TokenLocation location = current_token_.location();
    (void) advance();

    if (match(TokenType::Database)) {
        auto if_not_exists = parse_if_not_exists();
        if (!if_not_exists.has_value()) {
            return std::unexpected(if_not_exists.error());
        }
        auto database = parse_identifier_string("Expected database name");
        if (!database.has_value()) {
            return std::unexpected(database.error());
        }

        return std::make_unique<ast::CreateDatabaseStatement>(
            std::move(database.value()),
            if_not_exists.value(),
            ast_location(location)
        );
    }

    if (match(TokenType::Collection)) {
        auto if_not_exists = parse_if_not_exists();
        if (!if_not_exists.has_value()) {
            return std::unexpected(if_not_exists.error());
        }
        auto collection = parse_identifier_string("Expected collection name");
        if (!collection.has_value()) {
            return std::unexpected(collection.error());
        }

        auto left_paren = consume(TokenType::LeftParen, "Expected '(' before column definitions");
        if (!left_paren.has_value()) {
            return std::unexpected(left_paren.error());
        }
        if (check(TokenType::RightParen)) {
            return std::unexpected(ParserError {current_token_.location(), "Expected at least one column definition"});
        }

        ast::ColumnDefinitionList columns;
        while (true) {
            auto column = parse_column_definition();
            if (!column.has_value()) {
                return std::unexpected(column.error());
            }
            columns.push_back(std::move(column.value()));

            if (!match(TokenType::Comma)) {
                break;
            }
        }

        auto right_paren = consume(TokenType::RightParen, "Expected ')' after column definitions");
        if (!right_paren.has_value()) {
            return std::unexpected(right_paren.error());
        }

        return std::make_unique<ast::CreateCollectionStatement>(
            std::move(collection.value()),
            if_not_exists.value(),
            std::move(columns),
            ast_location(location)
        );
    }

    return std::unexpected(ParserError {current_token_.location(), "Expected DATABASE or COLLECTION after CREATE"});
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> Parser::parse_drop_statement()
{
    const TokenLocation location = current_token_.location();
    (void) advance();

    auto object_type = parse_schema_object_type(false);
    if (!object_type.has_value()) {
        return std::unexpected(object_type.error());
    }

    auto if_exists = parse_if_exists();
    if (!if_exists.has_value()) {
        return std::unexpected(if_exists.error());
    }
    auto name = parse_identifier_string("Expected object name");
    if (!name.has_value()) {
        return std::unexpected(name.error());
    }

    return std::make_unique<ast::DropStatement>(
        object_type.value(),
        std::move(name.value()),
        if_exists.value(),
        ast_location(location)
    );
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> Parser::parse_show_statement()
{
    const TokenLocation location = current_token_.location();
    (void) advance();

    auto object_type = parse_schema_object_type(true);
    if (!object_type.has_value()) {
        return std::unexpected(object_type.error());
    }

    return std::make_unique<ast::ShowStatement>(object_type.value(), ast_location(location));
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> Parser::parse_describe_statement()
{
    const TokenLocation location = current_token_.location();
    (void) advance();

    if (check(TokenType::Collection)) {
        (void) advance();
    }

    auto collection = parse_identifier_string("Expected collection name");
    if (!collection.has_value()) {
        return std::unexpected(collection.error());
    }

    return std::make_unique<ast::DescribeStatement>(
        ast::SchemaObjectType::Collection,
        std::move(collection.value()),
        ast_location(location)
    );
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> Parser::parse_insert_statement()
{
    const TokenLocation location = current_token_.location();
    (void) advance();

    auto into = consume(TokenType::Into, "Expected INTO after INSERT");
    if (!into.has_value()) {
        return std::unexpected(into.error());
    }

    auto collection = parse_identifier_string("Expected collection name");
    if (!collection.has_value()) {
        return std::unexpected(collection.error());
    }

    ast::InsertStatement::ColumnList columns;
    if (match(TokenType::LeftParen)) {
        if (check(TokenType::RightParen)) {
            return std::unexpected(ParserError {current_token_.location(), "Expected at least one column name"});
        }

        while (true) {
            auto column = parse_identifier_string("Expected column name");
            if (!column.has_value()) {
                return std::unexpected(column.error());
            }
            columns.push_back(std::move(column.value()));

            if (!match(TokenType::Comma)) {
                break;
            }
        }

        auto right_paren = consume(TokenType::RightParen, "Expected ')' after column list");
        if (!right_paren.has_value()) {
            return std::unexpected(right_paren.error());
        }
    }

    auto values = consume(TokenType::Values, "Expected VALUES after INSERT target");
    if (!values.has_value()) {
        return std::unexpected(values.error());
    }
    auto left_paren = consume(TokenType::LeftParen, "Expected '(' before values");
    if (!left_paren.has_value()) {
        return std::unexpected(left_paren.error());
    }
    if (check(TokenType::RightParen)) {
        return std::unexpected(ParserError {current_token_.location(), "Expected at least one value"});
    }

    ast::InsertStatement::ValueList value_list;
    while (true) {
        auto value = parse_expression();
        if (!value.has_value()) {
            return std::unexpected(value.error());
        }
        value_list.push_back(std::move(value.value()));

        if (!match(TokenType::Comma)) {
            break;
        }
    }

    auto right_paren = consume(TokenType::RightParen, "Expected ')' after values");
    if (!right_paren.has_value()) {
        return std::unexpected(right_paren.error());
    }

    return std::make_unique<ast::InsertStatement>(
        std::move(collection.value()),
        std::move(columns),
        std::move(value_list),
        ast_location(location)
    );
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> Parser::parse_update_statement()
{
    const TokenLocation location = current_token_.location();
    (void) advance();

    auto collection = parse_identifier_string("Expected collection name");
    if (!collection.has_value()) {
        return std::unexpected(collection.error());
    }

    auto set = consume(TokenType::Set, "Expected SET after collection name");
    if (!set.has_value()) {
        return std::unexpected(set.error());
    }

    ast::UpdateStatement::AssignmentList assignments;
    while (true) {
        auto column = parse_identifier_string("Expected column name");
        if (!column.has_value()) {
            return std::unexpected(column.error());
        }

        auto equal = consume(TokenType::Equal, "Expected '=' after column name");
        if (!equal.has_value()) {
            return std::unexpected(equal.error());
        }

        auto value = parse_expression();
        if (!value.has_value()) {
            return std::unexpected(value.error());
        }

        assignments.push_back(ast::Assignment {
            std::move(column.value()),
            std::move(value.value()),
        });

        if (!match(TokenType::Comma)) {
            break;
        }
    }

    std::unique_ptr<ast::ExpressionNode> where;
    if (match(TokenType::Where)) {
        auto expression = parse_expression();
        if (!expression.has_value()) {
            return std::unexpected(expression.error());
        }
        where = std::move(expression.value());
    }

    return std::make_unique<ast::UpdateStatement>(
        std::move(collection.value()),
        std::move(assignments),
        std::move(where),
        ast_location(location)
    );
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> Parser::parse_delete_statement()
{
    const TokenLocation location = current_token_.location();
    (void) advance();

    auto from = consume(TokenType::From, "Expected FROM after DELETE");
    if (!from.has_value()) {
        return std::unexpected(from.error());
    }

    auto collection = parse_identifier_string("Expected collection name");
    if (!collection.has_value()) {
        return std::unexpected(collection.error());
    }

    std::unique_ptr<ast::ExpressionNode> where;
    if (match(TokenType::Where)) {
        auto expression = parse_expression();
        if (!expression.has_value()) {
            return std::unexpected(expression.error());
        }
        where = std::move(expression.value());
    }

    return std::make_unique<ast::DeleteStatement>(
        std::move(collection.value()),
        std::move(where),
        ast_location(location)
    );
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> Parser::parse_select_statement()
{
    const TokenLocation location = current_token_.location();
    (void) advance();

    ast::SelectStatement::SelectList select_list;
    while (true) {
        auto item = parse_wildcard_or_column_reference();
        if (!item.has_value()) {
            return std::unexpected(item.error());
        }
        select_list.push_back(std::move(item.value()));

        if (!match(TokenType::Comma)) {
            break;
        }
    }

    auto from = consume(TokenType::From, "Expected FROM after select list");
    if (!from.has_value()) {
        return std::unexpected(from.error());
    }

    auto collection = parse_identifier_string("Expected collection name");
    if (!collection.has_value()) {
        return std::unexpected(collection.error());
    }

    std::unique_ptr<ast::ExpressionNode> where;
    if (match(TokenType::Where)) {
        auto expression = parse_expression();
        if (!expression.has_value()) {
            return std::unexpected(expression.error());
        }
        where = std::move(expression.value());
    }

    ast::SelectStatement::OrderByList order_by;
    if (match(TokenType::Order)) {
        auto by = consume(TokenType::By, "Expected BY after ORDER");
        if (!by.has_value()) {
            return std::unexpected(by.error());
        }

        while (true) {
            auto expression = parse_column_reference_expression();
            if (!expression.has_value()) {
                return std::unexpected(expression.error());
            }

            bool ascending = true;
            if (match(TokenType::Asc)) {
                ascending = true;
            } else if (match(TokenType::Desc)) {
                ascending = false;
            }

            order_by.push_back(ast::OrderByItem {std::move(expression.value()), ascending});

            if (!match(TokenType::Comma)) {
                break;
            }
        }
    }

    std::optional<std::size_t> limit;
    if (match(TokenType::Limit)) {
        auto value = parse_integer_value("Expected LIMIT value");
        if (!value.has_value()) {
            return std::unexpected(value.error());
        }
        limit = value.value();
    }

    std::optional<std::size_t> offset;
    if (match(TokenType::Offset)) {
        auto value = parse_integer_value("Expected OFFSET value");
        if (!value.has_value()) {
            return std::unexpected(value.error());
        }
        offset = value.value();
    }

    return std::make_unique<ast::SelectStatement>(
        std::move(select_list),
        std::move(collection.value()),
        std::move(where),
        std::move(order_by),
        limit,
        offset,
        ast_location(location)
    );
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> Parser::parse_expression()
{
    return parse_or_expression();
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> Parser::parse_or_expression()
{
    auto left = parse_and_expression();
    if (!left.has_value()) {
        return std::unexpected(left.error());
    }

    while (check(TokenType::Or)) {
        const Token op = advance();
        auto right = parse_and_expression();
        if (!right.has_value()) {
            return std::unexpected(right.error());
        }
        left = std::make_unique<ast::BinaryExpression>(
            std::move(left.value()),
            op.type(),
            std::move(right.value()),
            ast_location(op.location())
        );
    }

    return std::move(left.value());
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> Parser::parse_and_expression()
{
    auto left = parse_not_expression();
    if (!left.has_value()) {
        return std::unexpected(left.error());
    }

    while (check(TokenType::And)) {
        const Token op = advance();
        auto right = parse_not_expression();
        if (!right.has_value()) {
            return std::unexpected(right.error());
        }
        left = std::make_unique<ast::BinaryExpression>(
            std::move(left.value()),
            op.type(),
            std::move(right.value()),
            ast_location(op.location())
        );
    }

    return std::move(left.value());
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> Parser::parse_not_expression()
{
    if (check(TokenType::Not)) {
        const Token op = advance();
        auto operand = parse_not_expression();
        if (!operand.has_value()) {
            return std::unexpected(operand.error());
        }
        return std::make_unique<ast::UnaryExpression>(
            op.type(),
            std::move(operand.value()),
            ast_location(op.location())
        );
    }

    return parse_comparison_expression();
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> Parser::parse_comparison_expression()
{
    auto left = parse_additive_expression();
    if (!left.has_value()) {
        return std::unexpected(left.error());
    }

    if (is_comparison_operator(current_token_.type())) {
        const Token op = advance();
        auto right = parse_additive_expression();
        if (!right.has_value()) {
            return std::unexpected(right.error());
        }
        return std::make_unique<ast::BinaryExpression>(
            std::move(left.value()),
            op.type(),
            std::move(right.value()),
            ast_location(op.location())
        );
    }

    bool negated = false;
    TokenLocation not_location = current_token_.location();
    if (check(TokenType::Not)) {
        negated = true;
        not_location = advance().location();
    }

    if (check(TokenType::Like)) {
        const Token op = advance();
        auto pattern = parse_additive_expression();
        if (!pattern.has_value()) {
            return std::unexpected(pattern.error());
        }

        std::unique_ptr<ast::ExpressionNode> expression = std::make_unique<ast::LikeExpression>(
            std::move(left.value()),
            std::move(pattern.value()),
            ast_location(op.location())
        );
        if (negated) {
            expression = std::make_unique<ast::UnaryExpression>(
                TokenType::Not,
                std::move(expression),
                ast_location(not_location)
            );
        }
        return expression;
    }

    if (check(TokenType::In)) {
        const Token op = advance();
        auto left_paren = consume(TokenType::LeftParen, "Expected '(' after IN");
        if (!left_paren.has_value()) {
            return std::unexpected(left_paren.error());
        }
        if (check(TokenType::RightParen)) {
            return std::unexpected(ParserError {current_token_.location(), "Expected at least one IN value"});
        }

        ast::InExpression::ValueList values;
        while (true) {
            auto value = parse_expression();
            if (!value.has_value()) {
                return std::unexpected(value.error());
            }
            values.push_back(std::move(value.value()));

            if (!match(TokenType::Comma)) {
                break;
            }
        }

        auto right_paren = consume(TokenType::RightParen, "Expected ')' after IN values");
        if (!right_paren.has_value()) {
            return std::unexpected(right_paren.error());
        }

        std::unique_ptr<ast::ExpressionNode> expression = std::make_unique<ast::InExpression>(
            std::move(left.value()),
            std::move(values),
            ast_location(op.location())
        );
        if (negated) {
            expression = std::make_unique<ast::UnaryExpression>(
                TokenType::Not,
                std::move(expression),
                ast_location(not_location)
            );
        }
        return expression;
    }

    if (check(TokenType::Between)) {
        const Token op = advance();
        auto lower = parse_additive_expression();
        if (!lower.has_value()) {
            return std::unexpected(lower.error());
        }
        auto and_token = consume(TokenType::And, "Expected AND in BETWEEN expression");
        if (!and_token.has_value()) {
            return std::unexpected(and_token.error());
        }
        auto upper = parse_additive_expression();
        if (!upper.has_value()) {
            return std::unexpected(upper.error());
        }

        std::unique_ptr<ast::ExpressionNode> expression = std::make_unique<ast::BetweenExpression>(
            std::move(left.value()),
            std::move(lower.value()),
            std::move(upper.value()),
            ast_location(op.location())
        );
        if (negated) {
            expression = std::make_unique<ast::UnaryExpression>(
                TokenType::Not,
                std::move(expression),
                ast_location(not_location)
            );
        }
        return expression;
    }

    if (negated) {
        return std::unexpected(ParserError {current_token_.location(), "Expected LIKE, IN, or BETWEEN after NOT"});
    }

    return std::move(left.value());
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> Parser::parse_additive_expression()
{
    auto left = parse_multiplicative_expression();
    if (!left.has_value()) {
        return std::unexpected(left.error());
    }

    while (check(TokenType::Plus) || check(TokenType::Minus)) {
        const Token op = advance();
        auto right = parse_multiplicative_expression();
        if (!right.has_value()) {
            return std::unexpected(right.error());
        }
        left = std::make_unique<ast::BinaryExpression>(
            std::move(left.value()),
            op.type(),
            std::move(right.value()),
            ast_location(op.location())
        );
    }

    return std::move(left.value());
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> Parser::parse_multiplicative_expression()
{
    auto left = parse_unary_expression();
    if (!left.has_value()) {
        return std::unexpected(left.error());
    }

    while (check(TokenType::Star) || check(TokenType::Slash) || check(TokenType::Modulo)) {
        const Token op = advance();
        auto right = parse_unary_expression();
        if (!right.has_value()) {
            return std::unexpected(right.error());
        }
        left = std::make_unique<ast::BinaryExpression>(
            std::move(left.value()),
            op.type(),
            std::move(right.value()),
            ast_location(op.location())
        );
    }

    return std::move(left.value());
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> Parser::parse_unary_expression()
{
    if (check(TokenType::Plus) || check(TokenType::Minus)) {
        const Token op = advance();
        auto operand = parse_unary_expression();
        if (!operand.has_value()) {
            return std::unexpected(operand.error());
        }
        return std::make_unique<ast::UnaryExpression>(
            op.type(),
            std::move(operand.value()),
            ast_location(op.location())
        );
    }

    return parse_primary_expression();
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> Parser::parse_primary_expression()
{
    if (is_literal_token(current_token_.type())) {
        return parse_literal_expression();
    }
    if (check(TokenType::Identifier)) {
        return parse_column_reference_expression();
    }
    if (check(TokenType::LeftBracket)) {
        return parse_vector_expression();
    }
    if (match(TokenType::LeftParen)) {
        auto expression = parse_expression();
        if (!expression.has_value()) {
            return std::unexpected(expression.error());
        }

        auto right_paren = consume(TokenType::RightParen, "Expected ')' after expression");
        if (!right_paren.has_value()) {
            return std::unexpected(right_paren.error());
        }
        return std::move(expression.value());
    }

    return std::unexpected(ParserError {current_token_.location(), "Expected expression"});
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> Parser::parse_column_reference_expression()
{
    auto first = consume(TokenType::Identifier, "Expected column name");
    if (!first.has_value()) {
        return std::unexpected(first.error());
    }

    std::optional<std::string> qualifier;
    std::string column(first->value());
    if (match(TokenType::Dot)) {
        qualifier = std::move(column);
        auto second = consume(TokenType::Identifier, "Expected column name after '.'");
        if (!second.has_value()) {
            return std::unexpected(second.error());
        }
        column = std::string(second->value());
    }

    return std::make_unique<ast::ColumnReferenceExpression>(
        std::move(qualifier),
        std::move(column),
        ast_location(first->location())
    );
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> Parser::parse_wildcard_or_column_reference()
{
    if (check(TokenType::Star)) {
        const Token star = advance();
        return std::make_unique<ast::WildcardExpression>(ast_location(star.location()));
    }

    auto first = consume(TokenType::Identifier, "Expected select item");
    if (!first.has_value()) {
        return std::unexpected(first.error());
    }

    std::optional<std::string> qualifier;
    std::string column(first->value());
    if (match(TokenType::Dot)) {
        qualifier = std::move(column);
        if (check(TokenType::Star)) {
            (void) advance();
            return std::make_unique<ast::WildcardExpression>(
                std::move(qualifier),
                ast_location(first->location())
            );
        }

        auto second = consume(TokenType::Identifier, "Expected column name after '.'");
        if (!second.has_value()) {
            return std::unexpected(second.error());
        }
        column = std::string(second->value());
    }

    return std::make_unique<ast::ColumnReferenceExpression>(
        std::move(qualifier),
        std::move(column),
        ast_location(first->location())
    );
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> Parser::parse_literal_expression()
{
    if (check(TokenType::LeftBracket)) {
        return parse_vector_expression();
    }

    if (!is_literal_token(current_token_.type())) {
        return std::unexpected(ParserError {current_token_.location(), "Expected literal"});
    }

    const Token token = advance();
    return std::make_unique<ast::LiteralExpression>(
        token.type(),
        std::string(token.value()),
        ast_location(token.location())
    );
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> Parser::parse_vector_expression()
{
    const Token left_bracket = advance();
    if (check(TokenType::RightBracket)) {
        return std::unexpected(ParserError {current_token_.location(), "Expected at least one vector element"});
    }

    ast::VectorExpression::ElementList elements;
    while (true) {
        auto element = parse_expression();
        if (!element.has_value()) {
            return std::unexpected(element.error());
        }
        elements.push_back(std::move(element.value()));

        if (!match(TokenType::Comma)) {
            break;
        }
    }

    auto right_bracket = consume(TokenType::RightBracket, "Expected ']' after vector literal");
    if (!right_bracket.has_value()) {
        return std::unexpected(right_bracket.error());
    }

    return std::make_unique<ast::VectorExpression>(
        std::move(elements),
        ast_location(left_bracket.location())
    );
}

std::expected<std::string, ParserError> Parser::parse_identifier_string(std::string_view message)
{
    auto token = consume(TokenType::Identifier, message);
    if (!token.has_value()) {
        return std::unexpected(token.error());
    }

    return std::string(token->value());
}

std::expected<std::size_t, ParserError> Parser::parse_integer_value(std::string_view message)
{
    auto token = consume(TokenType::IntegerLiteral, message);
    if (!token.has_value()) {
        return std::unexpected(token.error());
    }

    std::size_t value = 0;
    const std::string_view text = token->value();
    const auto * begin = text.data();
    const auto * end = text.data() + text.size();
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc {} || result.ptr != end) {
        return std::unexpected(ParserError {token->location(), "Invalid integer value"});
    }

    return value;
}

std::expected<ast::DataType, ParserError> Parser::parse_data_type()
{
    if (match(TokenType::Integer)) {
        return ast::DataType {ast::DataTypeKind::Integer, std::nullopt};
    }
    if (match(TokenType::BigInt)) {
        return ast::DataType {ast::DataTypeKind::BigInt, std::nullopt};
    }
    if (match(TokenType::Float)) {
        return ast::DataType {ast::DataTypeKind::Float, std::nullopt};
    }
    if (match(TokenType::Double)) {
        return ast::DataType {ast::DataTypeKind::Double, std::nullopt};
    }
    if (match(TokenType::Boolean)) {
        return ast::DataType {ast::DataTypeKind::Boolean, std::nullopt};
    }
    if (match(TokenType::Varchar)) {
        auto left_paren = consume(TokenType::LeftParen, "Expected '(' after VARCHAR");
        if (!left_paren.has_value()) {
            return std::unexpected(left_paren.error());
        }
        auto parameter = parse_integer_value("Expected VARCHAR length");
        if (!parameter.has_value()) {
            return std::unexpected(parameter.error());
        }
        auto right_paren = consume(TokenType::RightParen, "Expected ')' after VARCHAR length");
        if (!right_paren.has_value()) {
            return std::unexpected(right_paren.error());
        }
        return ast::DataType {ast::DataTypeKind::Varchar, parameter.value()};
    }
    if (match(TokenType::Vector)) {
        auto left_paren = consume(TokenType::LeftParen, "Expected '(' after VECTOR");
        if (!left_paren.has_value()) {
            return std::unexpected(left_paren.error());
        }
        auto parameter = parse_integer_value("Expected VECTOR dimension");
        if (!parameter.has_value()) {
            return std::unexpected(parameter.error());
        }
        auto right_paren = consume(TokenType::RightParen, "Expected ')' after VECTOR dimension");
        if (!right_paren.has_value()) {
            return std::unexpected(right_paren.error());
        }
        return ast::DataType {ast::DataTypeKind::Vector, parameter.value()};
    }

    return std::unexpected(ParserError {current_token_.location(), "Expected data type"});
}

std::expected<ast::ColumnDefinition, ParserError> Parser::parse_column_definition()
{
    auto name = parse_identifier_string("Expected column name");
    if (!name.has_value()) {
        return std::unexpected(name.error());
    }

    auto type = parse_data_type();
    if (!type.has_value()) {
        return std::unexpected(type.error());
    }

    ast::ColumnDefinition column;
    column.name = std::move(name.value());
    column.type = type.value();

    while (!check(TokenType::Comma) && !check(TokenType::RightParen) && !check(TokenType::EoF)) {
        if (match(TokenType::Primary)) {
            auto key = consume(TokenType::Key, "Expected KEY after PRIMARY");
            if (!key.has_value()) {
                return std::unexpected(key.error());
            }
            column.primary_key = true;
        } else if (match(TokenType::Unique)) {
            column.unique = true;
        } else if (match(TokenType::Default)) {
            auto default_value = parse_literal_expression();
            if (!default_value.has_value()) {
                return std::unexpected(ParserError {current_token_.location(), "Expected literal after DEFAULT"});
            }
            column.default_value = std::move(default_value.value());
        } else if (match(TokenType::Comment)) {
            auto comment = consume(TokenType::StringLiteral, "Expected string literal after COMMENT");
            if (!comment.has_value()) {
                return std::unexpected(comment.error());
            }
            column.comment = std::string(comment->value());
        } else {
            return std::unexpected(ParserError {current_token_.location(), "Unexpected column constraint"});
        }
    }

    return column;
}

std::expected<ast::SchemaObjectType, ParserError> Parser::parse_schema_object_type(bool plural)
{
    if (!plural && match(TokenType::Database)) {
        return ast::SchemaObjectType::Database;
    }
    if (!plural && match(TokenType::Collection)) {
        return ast::SchemaObjectType::Collection;
    }
    if (plural && match(TokenType::Databases)) {
        return ast::SchemaObjectType::Database;
    }
    if (plural && match(TokenType::Collections)) {
        return ast::SchemaObjectType::Collection;
    }

    return std::unexpected(ParserError {
        current_token_.location(),
        plural ? "Expected DATABASES or COLLECTIONS" : "Expected DATABASE or COLLECTION"
    });
}

std::expected<bool, ParserError> Parser::parse_if_not_exists()
{
    if (!match(TokenType::If)) {
        return false;
    }

    auto not_token = consume(TokenType::Not, "Expected NOT after IF");
    if (!not_token.has_value()) {
        return std::unexpected(not_token.error());
    }
    auto exists_token = consume(TokenType::Exists, "Expected EXISTS after IF NOT");
    if (!exists_token.has_value()) {
        return std::unexpected(exists_token.error());
    }

    return true;
}

std::expected<bool, ParserError> Parser::parse_if_exists()
{
    if (!match(TokenType::If)) {
        return false;
    }

    auto exists_token = consume(TokenType::Exists, "Expected EXISTS after IF");
    if (!exists_token.has_value()) {
        return std::unexpected(exists_token.error());
    }

    return true;
}

Token Parser::advance()
{
    const Token previous = current_token_;
    current_token_ = lexer_->next();
    return previous;
}

bool Parser::match(TokenType type)
{
    if (!check(type)) {
        return false;
    }

    advance();
    return true;
}

bool Parser::check(TokenType type) const
{
    return current_token_.type() == type;
}

std::expected<Token, ParserError> Parser::consume(TokenType type, std::string_view message)
{
    if (!check(type)) {
        return std::unexpected(ParserError {current_token_.location(), std::string(message)});
    }

    return advance();
}

void Parser::skip_semicolon()
{
    if (check(TokenType::Semicolon)) {
        advance();
    }
}

ast::AstNodeLocation Parser::ast_location(TokenLocation location) const noexcept
{
    return ast::AstNodeLocation {location.line, location.column};
}

} // namespace litedb::core::parser
