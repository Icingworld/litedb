#include "core/parser/parser_mutation_worker.hpp"

#include <expected>
#include <memory>
#include <utility>

#include "core/parser/ast/statement/delete_statement.hpp"
#include "core/parser/ast/statement/insert_statement.hpp"
#include "core/parser/ast/statement/update_statement.hpp"
#include "core/parser/parser_expression_worker.hpp"
#include "core/parser/parser_schema_worker.hpp"

namespace litedb::core::parser
{

ParserMutationWorker::ParserMutationWorker(ParserContext & context)
    : context_(context)
    , expression_worker_(context)
    , schema_worker_(context)
{
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> ParserMutationWorker::parse_insert_statement()
{
    const TokenLocation location = context_.current().location();
    context_.advance();

    auto into = context_.consume(TokenType::Into, "Expected INTO after INSERT");
    if (!into.has_value()) [[unlikely]] {
        return std::unexpected(into.error());
    }

    auto collection = schema_worker_.parse_identifier_string("Expected collection name");
    if (!collection.has_value()) [[unlikely]] {
        return std::unexpected(collection.error());
    }

    ast::InsertStatement::ColumnList columns;
    if (context_.match(TokenType::LeftParen)) {
        if (context_.check(TokenType::RightParen)) [[unlikely]] {
            return std::unexpected(context_.make_current_error(ParserErrorCode::EmptyList, "Expected at least one column name"));
        }

        while (true) {
            auto column = schema_worker_.parse_identifier_string("Expected column name");
            if (!column.has_value()) [[unlikely]] {
                return std::unexpected(column.error());
            }
            columns.push_back(std::move(column.value()));

            if (!context_.match(TokenType::Comma)) {
                break;
            }
        }

        auto right_paren = context_.consume(TokenType::RightParen, "Expected ')' after column list");
        if (!right_paren.has_value()) [[unlikely]] {
            return std::unexpected(right_paren.error());
        }
    }

    auto values = context_.consume(TokenType::Values, "Expected VALUES after INSERT target");
    if (!values.has_value()) [[unlikely]] {
        return std::unexpected(values.error());
    }
    auto left_paren = context_.consume(TokenType::LeftParen, "Expected '(' before values");
    if (!left_paren.has_value()) [[unlikely]] {
        return std::unexpected(left_paren.error());
    }
    if (context_.check(TokenType::RightParen)) [[unlikely]] {
        return std::unexpected(context_.make_current_error(ParserErrorCode::EmptyList, "Expected at least one value"));
    }

    ast::InsertStatement::ValueList value_list;
    while (true) {
        auto value = expression_worker_.parse_expression();
        if (!value.has_value()) [[unlikely]] {
            return std::unexpected(value.error());
        }
        value_list.push_back(std::move(value.value()));

        if (!context_.match(TokenType::Comma)) {
            break;
        }
    }

    auto right_paren = context_.consume(TokenType::RightParen, "Expected ')' after values");
    if (!right_paren.has_value()) [[unlikely]] {
        return std::unexpected(right_paren.error());
    }

    return std::make_unique<ast::InsertStatement>(
        std::move(collection.value()),
        std::move(columns),
        std::move(value_list),
        context_.ast_location(location)
    );
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> ParserMutationWorker::parse_update_statement()
{
    const TokenLocation location = context_.current().location();
    context_.advance();

    auto collection = schema_worker_.parse_identifier_string("Expected collection name");
    if (!collection.has_value()) [[unlikely]] {
        return std::unexpected(collection.error());
    }

    auto set = context_.consume(TokenType::Set, "Expected SET after collection name");
    if (!set.has_value()) [[unlikely]] {
        return std::unexpected(set.error());
    }

    ast::UpdateStatement::AssignmentList assignments;
    while (true) {
        auto column = schema_worker_.parse_identifier_string("Expected column name");
        if (!column.has_value()) [[unlikely]] {
            return std::unexpected(column.error());
        }

        auto equal = context_.consume(TokenType::Equal, "Expected '=' after column name");
        if (!equal.has_value()) {
            return std::unexpected(equal.error());
        }

        auto value = expression_worker_.parse_expression();
        if (!value.has_value()) [[unlikely]] {
            return std::unexpected(value.error());
        }

        assignments.push_back(ast::Assignment {
            std::move(column.value()),
            std::move(value.value()),
        });

        if (!context_.match(TokenType::Comma)) {
            break;
        }
    }

    std::unique_ptr<ast::ExpressionNode> where;
    if (context_.match(TokenType::Where)) {
        auto expression = expression_worker_.parse_expression();
        if (!expression.has_value()) [[unlikely]] {
            return std::unexpected(expression.error());
        }
        where = std::move(expression.value());
    }

    return std::make_unique<ast::UpdateStatement>(
        std::move(collection.value()),
        std::move(assignments),
        std::move(where),
        context_.ast_location(location)
    );
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> ParserMutationWorker::parse_delete_statement()
{
    const TokenLocation location = context_.current().location();
    context_.advance();

    auto from = context_.consume(TokenType::From, "Expected FROM after DELETE");
    if (!from.has_value()) [[unlikely]] {
        return std::unexpected(from.error());
    }

    auto collection = schema_worker_.parse_identifier_string("Expected collection name");
    if (!collection.has_value()) [[unlikely]] {
        return std::unexpected(collection.error());
    }

    std::unique_ptr<ast::ExpressionNode> where;
    if (context_.match(TokenType::Where)) {
        auto expression = expression_worker_.parse_expression();
        if (!expression.has_value()) [[unlikely]] {
            return std::unexpected(expression.error());
        }
        where = std::move(expression.value());
    }

    return std::make_unique<ast::DeleteStatement>(
        std::move(collection.value()),
        std::move(where),
        context_.ast_location(location)
    );
}

} // namespace litedb::core::parser
