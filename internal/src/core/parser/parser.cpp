#include "core/parser/parser.hpp"

#include <charconv>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "core/parser/lexer.hpp"
#include "core/parser/ast/ast_node.hpp"
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
#include "core/parser/ast/schema.hpp"
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

/**
 * @brief 是否为比较运算符
 * @param type  Token 类型
 * @return 是否为比较运算符
 */
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

/**
 * @brief 是否为字面量 Token 
 * @param type  Token 类型
 * @return 是否为字面量 Token 
 */
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

/**
 * @brief 创建解析器错误
 * @param code 错误码
 * @param location 错误位置
 * @param message 错误消息
 * @return 解析器错误
 */
[[nodiscard]]
ParserError make_parser_error(ParserErrorCode code, TokenLocation location, std::string_view message)
{
    return ParserError {
        .code = code,
        .location = location,
        .message = std::string(message),
    };
}

/**
 * @brief 解析器工作器
 */
class ParserWorker
{
public:
    explicit ParserWorker(Lexer & lexer);

public:
    /**
     * @brief 解析SQL语句
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError> parse();

private:
    /**
     * @brief 解析语句
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError> parse_statement();

    /**
     * @brief 解析 USE 语句
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError> parse_use_statement();

    /**
     * @brief 解析 CREATE 语句
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError> parse_create_statement();

    /**
     * @brief 解析 DROP 语句
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError> parse_drop_statement();

    /**
     * @brief 解析 SHOW 语句
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError> parse_show_statement();

    /**
     * @brief 解析 DESCRIBE 语句
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError> parse_describe_statement();

    /**
     * @brief 解析 INSERT 语句
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError> parse_insert_statement();

    /**
     * @brief 解析 UPDATE 语句
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError> parse_update_statement();

    /**
     * @brief 解析 DELETE 语句
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError> parse_delete_statement();

    /**
     * @brief 解析 SELECT 语句
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError> parse_select_statement();

    /**
     * @brief 解析表达式
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> parse_expression();

    /**
     * @brief 解析 OR 表达式
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> parse_or_expression();

    /**
     * @brief 解析 AND 表达式
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> parse_and_expression();

    /**
     * @brief 解析 NOT 表达式
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> parse_not_expression();

    /**
     * @brief 解析比较表达式
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> parse_comparison_expression();

    /**
     * @brief 解析加法表达式
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> parse_additive_expression();

    /**
     * @brief 解析乘法表达式
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> parse_multiplicative_expression();

    /**
     * @brief 解析一元表达式
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> parse_unary_expression();

    /**
     * @brief 解析主表达式
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> parse_primary_expression();

    /**
     * @brief 解析列引用表达式
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> parse_column_reference_expression();

    /**
     * @brief 解析通配符或列引用表达式
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> parse_wildcard_or_column_reference();

    /**
     * @brief 解析字面量表达式
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> parse_literal_expression();

    /**
     * @brief 解析向量表达式
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> parse_vector_expression();

    /**
     * @brief 解析标识符字符串
     * @param message 错误消息
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::string, ParserError> parse_identifier_string(std::string_view message);

    /**
     * @brief 解析整数值
     * @param message 错误消息
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::size_t, ParserError> parse_integer_value(std::string_view message);

    /**
     * @brief 解析数据类型
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<ast::DataType, ParserError> parse_data_type();

    /**
     * @brief 解析列定义
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<ast::ColumnDefinition, ParserError> parse_column_definition();

    /**
     * @brief 解析模式对象类型
     * @param plural 是否为复数
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<ast::SchemaObjectType, ParserError> parse_schema_object_type(bool plural);

    /**
     * @brief 解析是否不存在
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<bool, ParserError> parse_if_not_exists();

    /**
     * @brief 解析是否存在
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<bool, ParserError> parse_if_exists();

    /**
     * @brief 前进一个 Token 
     * @return 前进后的 Token 
     */
    Token advance();

    /**
     * @brief 匹配 Token 类型
     * @param type  Token 类型
     * @return 是否匹配
     */
    bool match(TokenType type);

    /**
     * @brief 检查 Token 类型
     * @param type  Token 类型
     * @return 是否匹配
     */
    bool check(TokenType type) const;

    /**
     * @brief 创建当前位置的解析器错误
     * @param code 错误码
     * @param message 错误消息
     * @return 解析器错误
     */
    [[nodiscard]]
    ParserError make_current_error(ParserErrorCode code, std::string_view message) const;

    /**
     * @brief 消费 Token 
     * @param type  Token 类型
     * @param message 错误消息
     * @param code 错误码
     * @return 消费后的 Token 
     */
    [[nodiscard]]
    std::expected<Token, ParserError> consume(
        TokenType type,
        std::string_view message,
        ParserErrorCode code = ParserErrorCode::ExpectedToken
    );

    /**
     * @brief 跳过分号
     */
    void skip_semicolon();

    /**
     * @brief 从 Token 位置创建 AST 节点位置
     * @param location  Token 位置
     * @return AST节点位置
     */
    [[nodiscard]]
    ast::AstNodeLocation ast_location(TokenLocation location) const noexcept;

private:
    Lexer & lexer_;                 ///< 词法分析器
    Token current_token_;           ///< 当前 Token 
};

} // namespace

Parser::Parser(std::string input)
    : lexer_(std::make_unique<Lexer>(std::move(input)))
{
}

Parser::Parser(std::unique_ptr<Lexer> lexer)
    : lexer_(std::move(lexer))
{
}

Parser::~Parser() = default;

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> Parser::parse()
{
    // 使用 worker 解析 SQL 语句
    ParserWorker worker {*lexer_};
    return worker.parse();
}

ParserWorker::ParserWorker(Lexer & lexer)
    : lexer_(lexer)
    , current_token_(TokenType::EoF, "", TokenLocation {1, 1})
{
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> ParserWorker::parse()
{
    current_token_ = lexer_.next();

    // 检查是否出现空语句或错误 Token 
    if (current_token_.type() == TokenType::EoF) [[unlikely]] {
        return std::unexpected(make_current_error(ParserErrorCode::EmptyStatement, "Empty statement"));
    }
    if (current_token_.type() == TokenType::Error) [[unlikely]] {
        return std::unexpected(make_current_error(ParserErrorCode::LexicalError, "Invalid token"));
    }

    // 解析语句
    auto statement = parse_statement();
    if (!statement.has_value()) [[unlikely]] {
        return std::unexpected(statement.error());
    }

    // 跳过分号
    skip_semicolon();

    // 检查是否出现错误 Token 
    if (current_token_.type() == TokenType::Error) [[unlikely]] {
        return std::unexpected(make_current_error(ParserErrorCode::LexicalError, "Invalid token"));
    }
    if (current_token_.type() != TokenType::EoF) [[unlikely]] {
        return std::unexpected(make_current_error(ParserErrorCode::UnexpectedToken, "Unexpected token"));
    }

    return statement;
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> ParserWorker::parse_statement()
{
    // 根据当前 Token 类型分发到不同的解析
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
        [[fallthrough]];
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
    [[unlikely]] default:
        return std::unexpected(make_current_error(ParserErrorCode::UnexpectedStatement, "Unexpected statement"));
    }
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> ParserWorker::parse_use_statement()
{
    const TokenLocation location = current_token_.location();
    advance();

    auto database = parse_identifier_string("Expected database name");
    if (!database.has_value()) {
        return std::unexpected(database.error());
    }

    return std::make_unique<ast::UseStatement>(
        std::move(database.value()),
        ast_location(location)
    );
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> ParserWorker::parse_create_statement()
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
            return std::unexpected(make_current_error(ParserErrorCode::EmptyList, "Expected at least one column definition"));
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

    return std::unexpected(make_current_error(ParserErrorCode::UnsupportedSyntax, "Expected DATABASE or COLLECTION after CREATE"));
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> ParserWorker::parse_drop_statement()
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

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> ParserWorker::parse_show_statement()
{
    const TokenLocation location = current_token_.location();
    (void) advance();

    auto object_type = parse_schema_object_type(true);
    if (!object_type.has_value()) {
        return std::unexpected(object_type.error());
    }

    return std::make_unique<ast::ShowStatement>(object_type.value(), ast_location(location));
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> ParserWorker::parse_describe_statement()
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

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> ParserWorker::parse_insert_statement()
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
            return std::unexpected(make_current_error(ParserErrorCode::EmptyList, "Expected at least one column name"));
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
        return std::unexpected(make_current_error(ParserErrorCode::EmptyList, "Expected at least one value"));
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

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> ParserWorker::parse_update_statement()
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

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> ParserWorker::parse_delete_statement()
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

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> ParserWorker::parse_select_statement()
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

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> ParserWorker::parse_expression()
{
    return parse_or_expression();
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> ParserWorker::parse_or_expression()
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

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> ParserWorker::parse_and_expression()
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

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> ParserWorker::parse_not_expression()
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

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> ParserWorker::parse_comparison_expression()
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
            return std::unexpected(make_current_error(ParserErrorCode::EmptyList, "Expected at least one IN value"));
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
        return std::unexpected(make_current_error(ParserErrorCode::ExpectedToken, "Expected LIKE, IN, or BETWEEN after NOT"));
    }

    return std::move(left.value());
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> ParserWorker::parse_additive_expression()
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

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> ParserWorker::parse_multiplicative_expression()
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

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> ParserWorker::parse_unary_expression()
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

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> ParserWorker::parse_primary_expression()
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

    return std::unexpected(make_current_error(ParserErrorCode::ExpectedExpression, "Expected expression"));
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> ParserWorker::parse_column_reference_expression()
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

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> ParserWorker::parse_wildcard_or_column_reference()
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

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> ParserWorker::parse_literal_expression()
{
    if (check(TokenType::LeftBracket)) {
        return parse_vector_expression();
    }

    if (!is_literal_token(current_token_.type())) {
        return std::unexpected(make_current_error(ParserErrorCode::ExpectedLiteral, "Expected literal"));
    }

    const Token token = advance();
    return std::make_unique<ast::LiteralExpression>(
        token.type(),
        std::string(token.value()),
        ast_location(token.location())
    );
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> ParserWorker::parse_vector_expression()
{
    const Token left_bracket = advance();
    if (check(TokenType::RightBracket)) {
        return std::unexpected(make_current_error(ParserErrorCode::EmptyList, "Expected at least one vector element"));
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

std::expected<std::string, ParserError> ParserWorker::parse_identifier_string(std::string_view message)
{
    auto token = consume(TokenType::Identifier, message, ParserErrorCode::ExpectedIdentifier);
    if (!token.has_value()) {
        return std::unexpected(token.error());
    }

    return std::string(token->value());
}

std::expected<std::size_t, ParserError> ParserWorker::parse_integer_value(std::string_view message)
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
        return std::unexpected(make_parser_error(ParserErrorCode::InvalidInteger, token->location(), "Invalid integer value"));
    }

    return value;
}

std::expected<ast::DataType, ParserError> ParserWorker::parse_data_type()
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

    return std::unexpected(make_current_error(ParserErrorCode::ExpectedDataType, "Expected data type"));
}

std::expected<ast::ColumnDefinition, ParserError> ParserWorker::parse_column_definition()
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
                return std::unexpected(make_current_error(ParserErrorCode::ExpectedLiteral, "Expected literal after DEFAULT"));
            }
            column.default_value = std::move(default_value.value());
        } else if (match(TokenType::Comment)) {
            auto comment = consume(TokenType::StringLiteral, "Expected string literal after COMMENT");
            if (!comment.has_value()) {
                return std::unexpected(comment.error());
            }
            column.comment = std::string(comment->value());
        } else {
            return std::unexpected(make_current_error(ParserErrorCode::UnexpectedToken, "Unexpected column constraint"));
        }
    }

    return column;
}

std::expected<ast::SchemaObjectType, ParserError> ParserWorker::parse_schema_object_type(bool plural)
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

    return std::unexpected(make_current_error(
        ParserErrorCode::ExpectedToken,
        plural ? "Expected DATABASES or COLLECTIONS" : "Expected DATABASE or COLLECTION"
    ));
}

std::expected<bool, ParserError> ParserWorker::parse_if_not_exists()
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

std::expected<bool, ParserError> ParserWorker::parse_if_exists()
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

Token ParserWorker::advance()
{
    const Token previous = current_token_;
    current_token_ = lexer_.next();
    return previous;
}

bool ParserWorker::match(TokenType type)
{
    if (!check(type)) {
        return false;
    }

    advance();
    return true;
}

bool ParserWorker::check(TokenType type) const
{
    return current_token_.type() == type;
}

ParserError ParserWorker::make_current_error(ParserErrorCode code, std::string_view message) const
{
    if (current_token_.type() == TokenType::Error) {
        return make_parser_error(ParserErrorCode::LexicalError, current_token_.location(), "Invalid token");
    }

    return make_parser_error(code, current_token_.location(), message);
}

std::expected<Token, ParserError> ParserWorker::consume(
    TokenType type,
    std::string_view message,
    ParserErrorCode code
)
{
    if (!check(type)) {
        return std::unexpected(make_current_error(code, message));
    }

    return advance();
}

void ParserWorker::skip_semicolon()
{
    if (check(TokenType::Semicolon)) {
        advance();
    }
}

ast::AstNodeLocation ParserWorker::ast_location(TokenLocation location) const noexcept
{
    return ast::AstNodeLocation {location.line, location.column};
}

} // namespace litedb::core::parser
