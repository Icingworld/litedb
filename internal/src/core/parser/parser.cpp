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
#include "core/parser/ast/statement/create_index_statement.hpp"
#include "core/parser/ast/statement/delete_statement.hpp"
#include "core/parser/ast/statement/describe_statement.hpp"
#include "core/parser/ast/statement/drop_collection_statement.hpp"
#include "core/parser/ast/statement/drop_database_statement.hpp"
#include "core/parser/ast/statement/drop_index_statement.hpp"
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
     * @brief 匹配 Token 类型，成功则消耗
     * @param type  Token 类型
     * @return 是否匹配
     */
    bool match(TokenType type);

    /**
     * @brief 检查 Token 类型
     * @param type  Token 类型
     * @return 是否匹配
     */
    [[nodiscard]]
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
     * @brief 消费指定类型的 Token
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
    // 保存并消耗 USE 关键字
    const TokenLocation location = current_token_.location();
    advance();

    // 解析数据库名称
    auto database = parse_identifier_string("Expected database name");
    if (!database.has_value()) [[unlikely]] {
        return std::unexpected(database.error());
    }

    return std::make_unique<ast::UseStatement>(
        std::move(database.value()),
        ast_location(location)
    );
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> ParserWorker::parse_create_statement()
{
    // 保存并消耗 CREATE 关键字
    const TokenLocation location = current_token_.location();
    advance();

    // 尝试匹配 DATABASE 关键字
    if (match(TokenType::Database)) {
        // 解析 CREATE DATABASE 语句

        // 判断是否存在 IF NOT EXISTS 关键字
        auto if_not_exists = parse_if_not_exists();
        if (!if_not_exists.has_value()) [[unlikely]] {
            return std::unexpected(if_not_exists.error());
        }

        // 解析数据库名称
        auto database = parse_identifier_string("Expected database name");
        if (!database.has_value()) [[unlikely]] {
            return std::unexpected(database.error());
        }

        return std::make_unique<ast::CreateDatabaseStatement>(
            std::move(database.value()),
            if_not_exists.value(),
            ast_location(location)
        );
    }

    // 尝试匹配 COLLECTION 关键字
    if (match(TokenType::Collection)) {
        // 解析 CREATE COLLECTION 语句

        // 判断是否存在 IF NOT EXISTS 关键字
        auto if_not_exists = parse_if_not_exists();
        if (!if_not_exists.has_value()) [[unlikely]] {
            return std::unexpected(if_not_exists.error());
        }

        // 解析集合名称
        auto collection = parse_identifier_string("Expected collection name");
        if (!collection.has_value()) [[unlikely]] {
            return std::unexpected(collection.error());
        }

        // 期望 (
        auto left_paren = consume(TokenType::LeftParen, "Expected '(' before column definitions");
        if (!left_paren.has_value()) [[unlikely]] {
            return std::unexpected(left_paren.error());
        }
        // 检查是否为空列表
        if (check(TokenType::RightParen)) [[unlikely]] {
            return std::unexpected(make_current_error(ParserErrorCode::EmptyList, "Expected at least one column definition"));
        }

        // 解析列定义列表
        ast::ColumnDefinitionList columns;
        while (true) {
            auto column = parse_column_definition();
            if (!column.has_value()) [[unlikely]] {
                return std::unexpected(column.error());
            }
            columns.push_back(std::move(column.value()));

            // 列表元素之间期望使用逗号分隔
            if (!match(TokenType::Comma)) {
                break;
            }
        }

        // 期望 )
        auto right_paren = consume(TokenType::RightParen, "Expected ')' after column definitions");
        if (!right_paren.has_value()) [[unlikely]] {
            return std::unexpected(right_paren.error());
        }

        return std::make_unique<ast::CreateCollectionStatement>(
            std::move(collection.value()),
            if_not_exists.value(),
            std::move(columns),
            ast_location(location)
        );
    }

    // 匹配 INDEX 关键字
    if (match(TokenType::Index)) {
        // 解析 CREATE INDEX 语句

        // 判断是否存在 IF NOT EXISTS 关键字
        auto if_not_exists = parse_if_not_exists();
        if (!if_not_exists.has_value()) [[unlikely]] {
            return std::unexpected(if_not_exists.error());
        }   

        // 解析索引名称
        auto index_name = parse_identifier_string("Expected index name");
        if (!index_name.has_value()) [[unlikely]] {
            return std::unexpected(index_name.error());
        }   

        // 期望 ON 关键字
        auto on = consume(TokenType::On, "Expected ON after index name");
        if (!on.has_value()) [[unlikely]] {
            return std::unexpected(on.error());
        }

        // 解析集合名称
        auto collection_name = parse_identifier_string("Expected collection name");
        if (!collection_name.has_value()) [[unlikely]] {
            return std::unexpected(collection_name.error());
        }

        // 期望 (
        auto left_paren = consume(TokenType::LeftParen, "Expected '(' before index column");
        if (!left_paren.has_value()) [[unlikely]] {
            return std::unexpected(left_paren.error());
        }

        // 解析列名称
        auto column_name = parse_identifier_string("Expected index column name");
        if (!column_name.has_value()) [[unlikely]] {
            return std::unexpected(column_name.error());
        }

        // 期望 )
        auto right_paren = consume(TokenType::RightParen, "Expected ')' after index column");
        if (!right_paren.has_value()) [[unlikely]] {
            return std::unexpected(right_paren.error());
        }

        // 解析创建索引方法
        auto method = ast::CreateIndexMethod::Default;
        // 尝试匹配 USING 关键字
        if (match(TokenType::Using)) {
            // 解析创建索引方法
            if (match(TokenType::Hash)) {
                method = ast::CreateIndexMethod::Hash;
            } else if (match(TokenType::BTree)) {
                method = ast::CreateIndexMethod::BTree;
            } else [[unlikely]] {
                return std::unexpected(make_parser_error(
                    ParserErrorCode::UnsupportedSyntax,
                    current_token_.location(),
                    "Expected HASH or BTREE after USING"
                ));
            }
        }

        return std::make_unique<ast::CreateIndexStatement>(
            std::move(index_name.value()),
            std::move(collection_name.value()),
            std::move(column_name.value()),
            if_not_exists.value(),
            method,
            ast_location(location)
        );
    }

    [[unlikely]] return std::unexpected(make_current_error(
        ParserErrorCode::UnsupportedSyntax, 
        "Expected DATABASE, COLLECTION, or INDEX after CREATE"
    ));
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> ParserWorker::parse_drop_statement()
{
    // 保存并消耗 DROP 关键字
    const TokenLocation location = current_token_.location();
    advance();

    if (match(TokenType::Database)) {
        // 解析 DROP DATABASE 语句

        // 判断是否存在 IF EXISTS 关键字
        auto if_exists = parse_if_exists();
        if (!if_exists.has_value()) [[unlikely]] {
            return std::unexpected(if_exists.error());
        }

        // 解析数据库名称
        auto database = parse_identifier_string("Expected database name");
        if (!database.has_value()) [[unlikely]] {
            return std::unexpected(database.error());
        }

        return std::make_unique<ast::DropDatabaseStatement>(
            std::move(database.value()),
            if_exists.value(),
            ast_location(location)
        );
    }

    if (match(TokenType::Collection)) {
        // 解析 DROP COLLECTION 语句

        // 判断是否存在 IF EXISTS 关键字
        auto if_exists = parse_if_exists();
        if (!if_exists.has_value()) [[unlikely]] {
            return std::unexpected(if_exists.error());
        }

        // 解析集合名称
        auto collection = parse_identifier_string("Expected collection name");
        if (!collection.has_value()) [[unlikely]] {
            return std::unexpected(collection.error());
        }

        return std::make_unique<ast::DropCollectionStatement>(
            std::move(collection.value()),
            if_exists.value(),
            ast_location(location)
        );
    }

    if (match(TokenType::Index)) {
        // 解析 DROP INDEX 语句

        // 判断是否存在 IF EXISTS 关键字
        auto if_exists = parse_if_exists();
        if (!if_exists.has_value()) [[unlikely]] {
            return std::unexpected(if_exists.error());
        }

        // 解析索引名称
        auto index_name = parse_identifier_string("Expected index name");
        if (!index_name.has_value()) [[unlikely]] {
            return std::unexpected(index_name.error());
        }

        return std::make_unique<ast::DropIndexStatement>(
            std::move(index_name.value()),
            if_exists.value(),
            ast_location(location)
        );
    }

    return std::unexpected(make_current_error(
        ParserErrorCode::ExpectedToken,
        "Expected DATABASE, COLLECTION, or INDEX after DROP"
    ));
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> ParserWorker::parse_show_statement()
{
    // 保存并消耗 SHOW 关键字
    const TokenLocation location = current_token_.location();
    advance();

    // 解析对象类型
    auto object_type = parse_schema_object_type(true);
    if (!object_type.has_value()) [[unlikely]] {
        return std::unexpected(object_type.error());
    }

    return std::make_unique<ast::ShowStatement>(object_type.value(), ast_location(location));
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> ParserWorker::parse_describe_statement()
{
    // 保存并消耗 DESCRIBE 关键字
    const TokenLocation location = current_token_.location();
    advance();

    // 判断是否为 COLLECTION 关键字
    if (check(TokenType::Collection)) {
        advance();
    }

    // 解析集合名称
    auto collection = parse_identifier_string("Expected collection name");
    if (!collection.has_value()) [[unlikely]] {
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
    // 保存并消耗 INSERT 关键字
    const TokenLocation location = current_token_.location();
    advance();

    // 期望 INTO 关键字
    auto into = consume(TokenType::Into, "Expected INTO after INSERT");
    if (!into.has_value()) [[unlikely]] {
        return std::unexpected(into.error());
    }

    // 解析集合名称
    auto collection = parse_identifier_string("Expected collection name");
    if (!collection.has_value()) [[unlikely]] {
        return std::unexpected(collection.error());
    }

    // 解析列列表
    ast::InsertStatement::ColumnList columns;
    // 尝试匹配 (
    if (match(TokenType::LeftParen)) {
        // 检查是否为空列表
        if (check(TokenType::RightParen)) [[unlikely]] {
            return std::unexpected(make_current_error(ParserErrorCode::EmptyList, "Expected at least one column name"));
        }

        // 解析列名称列表
        while (true) {
            // 解析列名称
            auto column = parse_identifier_string("Expected column name");
            if (!column.has_value()) [[unlikely]] {
                return std::unexpected(column.error());
            }
            columns.push_back(std::move(column.value()));

            // 列表元素之间期望使用逗号分隔
            if (!match(TokenType::Comma)) {
                break;
            }
        }

        // 期望 )
        auto right_paren = consume(TokenType::RightParen, "Expected ')' after column list");
        if (!right_paren.has_value()) [[unlikely]] {
            return std::unexpected(right_paren.error());
        }
    }

    // 期望 VALUES 关键字
    auto values = consume(TokenType::Values, "Expected VALUES after INSERT target");
    if (!values.has_value()) [[unlikely]] {
        return std::unexpected(values.error());
    }
    // 期望 (
    auto left_paren = consume(TokenType::LeftParen, "Expected '(' before values");
    if (!left_paren.has_value()) [[unlikely]] {
        return std::unexpected(left_paren.error());
    }
    // 检查是否为空列表
    if (check(TokenType::RightParen)) [[unlikely]] {
        return std::unexpected(make_current_error(ParserErrorCode::EmptyList, "Expected at least one value"));
    }

    // 解析值列表
    ast::InsertStatement::ValueList value_list;
    while (true) {
        // 解析表达式
        auto value = parse_expression();
        if (!value.has_value()) [[unlikely]] {
            return std::unexpected(value.error());
        }
        value_list.push_back(std::move(value.value()));

        // 列表元素之间期望使用逗号分隔
        if (!match(TokenType::Comma)) {
            break;
        }
    }

    // 期望 )
    auto right_paren = consume(TokenType::RightParen, "Expected ')' after values");
    if (!right_paren.has_value()) [[unlikely]] {
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
    // 保存并消耗 UPDATE 关键字
    const TokenLocation location = current_token_.location();
    advance();

    // 解析集合名称
    auto collection = parse_identifier_string("Expected collection name");
    if (!collection.has_value()) [[unlikely]] {
        return std::unexpected(collection.error());
    }

    // 期望 SET 关键字
    auto set = consume(TokenType::Set, "Expected SET after collection name");
    if (!set.has_value()) [[unlikely]] {
        return std::unexpected(set.error());
    }

    // 解析赋值列表
    ast::UpdateStatement::AssignmentList assignments;
    while (true) {
        // 解析列名称
        auto column = parse_identifier_string("Expected column name");
        if (!column.has_value()) [[unlikely]] {
            return std::unexpected(column.error());
        }

        // 期望 = 关键字
        auto equal = consume(TokenType::Equal, "Expected '=' after column name");
        if (!equal.has_value()) {
            return std::unexpected(equal.error());
        }

        // 解析表达式
        auto value = parse_expression();
        if (!value.has_value()) [[unlikely]] {
            return std::unexpected(value.error());
        }

        assignments.push_back(ast::Assignment {
            std::move(column.value()),
            std::move(value.value()),
        });

        // 列表元素之间期望使用逗号分隔
        if (!match(TokenType::Comma)) {
            break;
        }
    }

    // 尝试匹配 WHERE 关键字
    std::unique_ptr<ast::ExpressionNode> where;
    if (match(TokenType::Where)) {
        // 解析表达式
        auto expression = parse_expression();
        if (!expression.has_value()) [[unlikely]] {
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
    // 保存并消耗 DELETE 关键字
    const TokenLocation location = current_token_.location();
    advance();

    // 期望 FROM 关键字
    auto from = consume(TokenType::From, "Expected FROM after DELETE");
    if (!from.has_value()) [[unlikely]] {
        return std::unexpected(from.error());
    }

    // 解析集合名称
    auto collection = parse_identifier_string("Expected collection name");
    if (!collection.has_value()) [[unlikely]] {
        return std::unexpected(collection.error());
    }

    // 尝试匹配 WHERE 关键字
    std::unique_ptr<ast::ExpressionNode> where;
    if (match(TokenType::Where)) {
        // 解析表达式
        auto expression = parse_expression();
        if (!expression.has_value()) [[unlikely]] {
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
    // 保存并消耗 SELECT 关键字
    const TokenLocation location = current_token_.location();
    advance();

    // 解析选择列表
    ast::SelectStatement::SelectList select_list;
    while (true) {
        // 解析选择项，* 或列引用
        auto item = parse_wildcard_or_column_reference();
        if (!item.has_value()) [[unlikely]] {
            return std::unexpected(item.error());
        }
        select_list.push_back(std::move(item.value()));

        // 列表元素之间期望使用逗号分隔
        if (!match(TokenType::Comma)) {
            break;
        }
    }

    // 期望 FROM 关键字
    auto from = consume(TokenType::From, "Expected FROM after select list");
    if (!from.has_value()) [[unlikely]] {
        return std::unexpected(from.error());
    }

    // 解析集合名称
    auto collection = parse_identifier_string("Expected collection name");
    if (!collection.has_value()) [[unlikely]] {
        return std::unexpected(collection.error());
    }

    // 尝试匹配 WHERE 关键字
    std::unique_ptr<ast::ExpressionNode> where;
    if (match(TokenType::Where)) {
        // 解析表达式
        auto expression = parse_expression();
        if (!expression.has_value()) [[unlikely]] {
            return std::unexpected(expression.error());
        }
        where = std::move(expression.value());
    }

    // 解析排序列表
    ast::SelectStatement::OrderByList order_by;
    // 尝试匹配 ORDER BY 关键字
    if (match(TokenType::Order)) {
        auto by = consume(TokenType::By, "Expected BY after ORDER");
        if (!by.has_value()) [[unlikely]] {
            return std::unexpected(by.error());
        }

        // 解析排序项列表
        while (true) {
            // 解析表达式
            auto expression = parse_column_reference_expression();
            if (!expression.has_value()) [[unlikely]] {
                return std::unexpected(expression.error());
            }

            // 默认为升序排序
            bool ascending = true;
            if (match(TokenType::Asc)) {
                ascending = true;
            } else if (match(TokenType::Desc)) {
                ascending = false;
            }

            order_by.push_back(ast::OrderByItem {std::move(expression.value()), ascending});

            // 列表元素之间期望使用逗号分隔
            if (!match(TokenType::Comma)) {
                break;
            }
        }
    }

    // 尝试匹配 LIMIT 关键字
    std::optional<std::size_t> limit;
    if (match(TokenType::Limit)) {
        // 解析整数
        auto value = parse_integer_value("Expected LIMIT value");
        if (!value.has_value()) [[unlikely]] {
            return std::unexpected(value.error());
        }
        limit = value.value();
    }

    // 尝试匹配 OFFSET 关键字
    std::optional<std::size_t> offset;
    if (match(TokenType::Offset)) {
        // 解析整数
        auto value = parse_integer_value("Expected OFFSET value");
        if (!value.has_value()) [[unlikely]] {
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
    // 递归下降解析表达式
    // 完整的解析流程为：
    // parse_expression()
    //   └─> parse_or_expression()                          // OR，最低优先级逻辑运算符
    //         ├─> parse_and_expression()                   // 左侧；循环中遇到 OR 时解析右侧
    //         │     ├─> parse_not_expression()             // 左侧；循环中遇到 AND 时解析右侧
    //         │     │     ├─> [NOT] parse_not_expression() // 一元 NOT，右递归
    //         │     │     └─> parse_comparison_expression()
    //         │     │           ├─> parse_additive_expression()                              // 比较左操作数
    //         │     │           ├─> [==, !=, <, <=, >, >=] parse_additive_expression()       // 二元比较（仅一次）
    //         │     │           ├─> [NOT] LIKE parse_additive_expression()                   // 模式匹配，NOT 可选
    //         │     │           ├─> [NOT] IN ( parse_expression(), ... )                     // 集合成员，NOT 可选
    //         │     │           ├─> [NOT] BETWEEN ... AND ...                                // 范围，NOT 可选
    //         │     │           └─> 无后缀比较/匹配操作符时，直接返回左侧
    //         │     │                 └─> parse_additive_expression()                        // + / -
    //         │     │                       ├─> parse_multiplicative_expression()            // * / %
    //         │     │                       │     ├─> parse_unary_expression()               // 一元 + / -
    //         │     │                       │     │     ├─> [+, -] parse_unary_expression()  // 右递归
    //         │     │                       │     │     └─> parse_primary_expression()
    //         │     │                       │     │           ├─> parse_literal_expression()           // 字面量
    //         │     │                       │     │           ├─> parse_column_reference_expression()  // 列引用（支持 表.列）
    //         │     │                       │     │           ├─> parse_vector_expression()            // 向量字面量 [...]
    //         │     │                       │     │           └─> ( parse_expression() )               // 括号分组，回到顶层
    //         │     │                       │     └─> 循环：* / % 与右侧 parse_unary_expression()
    //         │     │                       └─> 循环：+ / - 与右侧 parse_multiplicative_expression()
    //         │     └─> 循环：AND 与右侧 parse_not_expression()
    //         └─> 循环：OR 与右侧 parse_and_expression()

    return parse_or_expression();
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> ParserWorker::parse_or_expression()
{
    // 解析左侧 AND 表达式
    auto left = parse_and_expression();
    if (!left.has_value()) [[unlikely]] {
        return std::unexpected(left.error());
    }

    // 左结合循环处理 OR 表达式
    while (check(TokenType::Or)) {
        // 消耗 OR 关键字
        const Token op = advance();

        // 解析右侧 AND 表达式
        auto right = parse_and_expression();
        if (!right.has_value()) [[unlikely]] {
            return std::unexpected(right.error());
        }

        left = std::make_unique<ast::BinaryExpression>(
            std::move(left.value()),
            op.type(),
            std::move(right.value()),
            ast_location(op.location())
        );
    }

    return left;
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> ParserWorker::parse_and_expression()
{
    // 解析左侧 NOT 表达式
    auto left = parse_not_expression();
    if (!left.has_value()) [[unlikely]] {
        return std::unexpected(left.error());
    }

    // 左结合循环处理 AND 表达式
    while (check(TokenType::And)) {
        // 消耗 AND 关键字
        const Token op = advance();

        // 解析右侧 NOT 表达式
        auto right = parse_not_expression();
        if (!right.has_value()) [[unlikely]] {
            return std::unexpected(right.error());
        }

        left = std::make_unique<ast::BinaryExpression>(
            std::move(left.value()),
            op.type(),
            std::move(right.value()),
            ast_location(op.location())
        );
    }

    return left;
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> ParserWorker::parse_not_expression()
{
    // 尝试匹配 NOT 关键字
    if (check(TokenType::Not)) {
        // 消耗 NOT 关键字
        const Token op = advance();

        // 继续解析 NOT 表达式
        auto operand = parse_not_expression();
        if (!operand.has_value()) [[unlikely]] {
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
    // 解析左侧加法表达式
    auto left = parse_additive_expression();
    if (!left.has_value()) [[unlikely]] {
        return std::unexpected(left.error());
    }

    // 尝试匹配比较运算符
    if (is_comparison_operator(current_token_.type())) {
        // 消耗比较运算符
        const Token op = advance();

        // 解析右侧加法表达式
        auto right = parse_additive_expression();
        if (!right.has_value()) [[unlikely]] {
            return std::unexpected(right.error());
        }

        return std::make_unique<ast::BinaryExpression>(
            std::move(left.value()),
            op.type(),
            std::move(right.value()),
            ast_location(op.location())
        );
    }

    // 尝试匹配 NOT 关键字
    bool negated = false;
    TokenLocation not_location = current_token_.location();
    if (check(TokenType::Not)) {
        // 设置 NOT 标志
        negated = true;
        not_location = advance().location();
    }

    // 尝试匹配 LIKE 关键字
    if (check(TokenType::Like)) {
        // 消耗 LIKE 关键字
        const Token op = advance();

        // 解析右侧加法表达式
        auto pattern = parse_additive_expression();
        if (!pattern.has_value()) [[unlikely]] {
            return std::unexpected(pattern.error());
        }

        std::unique_ptr<ast::ExpressionNode> expression = std::make_unique<ast::LikeExpression>(
            std::move(left.value()),
            std::move(pattern.value()),
            ast_location(op.location())
        );
        // 如果设置了 NOT 标志，实际上创建一个一元表达式
        if (negated) {
            expression = std::make_unique<ast::UnaryExpression>(
                TokenType::Not,
                std::move(expression),
                ast_location(not_location)
            );
        }
        return expression;
    }

    // 尝试匹配 IN 关键字
    if (check(TokenType::In)) {
        // 消耗 IN 关键字
        const Token op = advance();

        // 期望 (
        auto left_paren = consume(TokenType::LeftParen, "Expected '(' after IN");
        if (!left_paren.has_value()) [[unlikely]] {
            return std::unexpected(left_paren.error());
        }
        // 检查是否为空列表
        if (check(TokenType::RightParen)) [[unlikely]] {
            return std::unexpected(make_current_error(ParserErrorCode::EmptyList, "Expected at least one IN value"));
        }

        // 解析值列表
        ast::InExpression::ValueList values;
        // 循环解析值
        while (true) {
            // 解析表达式
            auto value = parse_expression();
            if (!value.has_value()) [[unlikely]] {
                return std::unexpected(value.error());
            }
            values.push_back(std::move(value.value()));

            // 列表元素之间期望使用逗号分隔
            if (!match(TokenType::Comma)) {
                break;
            }
        }

        // 期望 )
        auto right_paren = consume(TokenType::RightParen, "Expected ')' after IN values");
        if (!right_paren.has_value()) [[unlikely]] {
            return std::unexpected(right_paren.error());
        }

        std::unique_ptr<ast::ExpressionNode> expression = std::make_unique<ast::InExpression>(
            std::move(left.value()),
            std::move(values),
            ast_location(op.location())
        );
        // 检测 NOT 标志
        if (negated) {
            expression = std::make_unique<ast::UnaryExpression>(
                TokenType::Not,
                std::move(expression),
                ast_location(not_location)
            );
        }
        return expression;
    }

    // 尝试匹配 BETWEEN 关键字
    if (check(TokenType::Between)) {
        // 消耗 BETWEEN 关键字
        const Token op = advance();

        // 解析下界表达式
        auto lower = parse_additive_expression();
        if (!lower.has_value()) [[unlikely]] {
            return std::unexpected(lower.error());
        }

        // 期望 AND
        auto and_token = consume(TokenType::And, "Expected AND in BETWEEN expression");
        if (!and_token.has_value()) [[unlikely]] {
            return std::unexpected(and_token.error());
        }

        // 解析上界表达式
        auto upper = parse_additive_expression();
        if (!upper.has_value()) [[unlikely]] {
            return std::unexpected(upper.error());
        }

        std::unique_ptr<ast::ExpressionNode> expression = std::make_unique<ast::BetweenExpression>(
            std::move(left.value()),
            std::move(lower.value()),
            std::move(upper.value()),
            ast_location(op.location())
        );
        // 检测 NOT 标志
        if (negated) {
            expression = std::make_unique<ast::UnaryExpression>(
                TokenType::Not,
                std::move(expression),
                ast_location(not_location)
            );
        }
        return expression;
    }

    // 不符合任何比较/匹配操作符，且设置了 NOT 标志，则错误
    if (negated) [[unlikely]] {
        return std::unexpected(make_current_error(ParserErrorCode::ExpectedToken, "Expected LIKE, IN, or BETWEEN after NOT"));
    }

    return left;
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> ParserWorker::parse_additive_expression()
{
    // 解析左侧乘法表达式
    auto left = parse_multiplicative_expression();
    if (!left.has_value()) [[unlikely]] {
        return std::unexpected(left.error());
    }

    // 左结合循环处理加法表达式
    while (check(TokenType::Plus) || check(TokenType::Minus)) {
        // 消耗加法运算符
        const Token op = advance();

        // 解析右侧乘法表达式
        auto right = parse_multiplicative_expression();
        if (!right.has_value()) [[unlikely]] {
            return std::unexpected(right.error());
        }

        left = std::make_unique<ast::BinaryExpression>(
            std::move(left.value()),
            op.type(),
            std::move(right.value()),
            ast_location(op.location())
        );
    }

    return left;
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> ParserWorker::parse_multiplicative_expression()
{
    // 解析左侧一元表达式
    auto left = parse_unary_expression();
    if (!left.has_value()) [[unlikely]] {
        return std::unexpected(left.error());
    }

    // 左结合循环处理乘法表达式
    while (check(TokenType::Star) || check(TokenType::Slash) || check(TokenType::Modulo)) {
        // 消耗乘法运算符
        const Token op = advance();

        // 解析右侧一元表达式
        auto right = parse_unary_expression();
        if (!right.has_value()) [[unlikely]] {
            return std::unexpected(right.error());
        }

        left = std::make_unique<ast::BinaryExpression>(
            std::move(left.value()),
            op.type(),
            std::move(right.value()),
            ast_location(op.location())
        );
    }

    return left;
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> ParserWorker::parse_unary_expression()
{
    // 尝试匹配 + 或 - 关键字
    if (check(TokenType::Plus) || check(TokenType::Minus)) {
        // 消耗 + 或 - 关键字
        const Token op = advance();

        // 解析右侧一元表达式
        auto operand = parse_unary_expression();
        if (!operand.has_value()) [[unlikely]] {
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
    // 根据不同的 Token 类型，解析不同的表达式

    // 尝试匹配字面量
    if (is_literal_token(current_token_.type())) {
        return parse_literal_expression();
    }
    // 尝试匹配列引用
    if (check(TokenType::Identifier)) {
        return parse_column_reference_expression();
    }
    // 遇到 [ 则尝试匹配向量字面量
    if (check(TokenType::LeftBracket)) {
        return parse_vector_expression();
    }
    // 遇到 ( 则尝试匹配括号分组
    if (match(TokenType::LeftParen)) {
        // 解析括号内的表达式
        auto expression = parse_expression();
        if (!expression.has_value()) [[unlikely]] {
            return std::unexpected(expression.error());
        }

        // 期望 )
        auto right_paren = consume(TokenType::RightParen, "Expected ')' after expression");
        if (!right_paren.has_value()) [[unlikely]] {
            return std::unexpected(right_paren.error());
        }

        return expression;
    }

    return std::unexpected(make_current_error(ParserErrorCode::ExpectedExpression, "Expected expression"));
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> ParserWorker::parse_column_reference_expression()
{
    // 期望标识符
    auto first = consume(TokenType::Identifier, "Expected column name");
    if (!first.has_value()) [[unlikely]] {
        return std::unexpected(first.error());
    }

    // 列名格式为 column 或 qualifier.column
    std::optional<std::string> qualifier;
    std::string column(first->value());

    // 尝试匹配 .
    if (match(TokenType::Dot)) {
        // 如果存在 . 则列名为 qualifier.column
        qualifier = std::move(column);

        // 期望标识符
        auto second = consume(TokenType::Identifier, "Expected column name after '.'");
        if (!second.has_value()) [[unlikely]] {
            return std::unexpected(second.error());
        }
        // 更新列名
        column = std::string(second->value());
    }

    // 创建列引用表达式
    return std::make_unique<ast::ColumnReferenceExpression>(
        std::move(qualifier),
        std::move(column),
        ast_location(first->location())
    );
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> ParserWorker::parse_wildcard_or_column_reference()
{
    // 尝试匹配 *
    if (check(TokenType::Star)) {
        // 消耗 *
        const Token star = advance();

        // 创建无限定符的通配符表达式
        return std::make_unique<ast::WildcardExpression>(ast_location(star.location()));
    }

    // 尝试匹配列引用
    auto first = consume(TokenType::Identifier, "Expected select item");
    if (!first.has_value()) [[unlikely]] {
        return std::unexpected(first.error());
    }

    // 列名格式为 qualifier.column
    std::optional<std::string> qualifier;
    std::string column(first->value());
    if (match(TokenType::Dot)) {
        qualifier = std::move(column);

        // 尝试匹配 *
        if (check(TokenType::Star)) {
            // 消耗 *
            advance();

            // 创建带限定符的通配符表达式
            return std::make_unique<ast::WildcardExpression>(
                std::move(qualifier),
                ast_location(first->location())
            );
        }

        // 期望标识符
        auto second = consume(TokenType::Identifier, "Expected column name after '.'");
        if (!second.has_value()) [[unlikely]] {
            return std::unexpected(second.error());
        }
        column = std::string(second->value());
    }

    // 创建普通的列引用表达式
    return std::make_unique<ast::ColumnReferenceExpression>(
        std::move(qualifier),
        std::move(column),
        ast_location(first->location())
    );
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> ParserWorker::parse_literal_expression()
{
    // 如果是 [ 则尝试匹配向量字面量
    if (check(TokenType::LeftBracket)) {
        return parse_vector_expression();
    }

    // 如果不是字面量 Token，则错误
    if (!is_literal_token(current_token_.type())) [[unlikely]] {
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
    // 消耗 [
    const Token left_bracket = advance();
    // 检查是否空列表
    if (check(TokenType::RightBracket)) [[unlikely]] {
        return std::unexpected(make_current_error(ParserErrorCode::EmptyList, "Expected at least one vector element"));
    }

    // 解析元素列表
    ast::VectorExpression::ElementList elements;
    // 循环解析元素
    while (true) {
        // 解析表达式
        auto element = parse_expression();
        if (!element.has_value()) [[unlikely]] {
            return std::unexpected(element.error());
        }
        elements.push_back(std::move(element.value()));

        // 列表元素之间期望使用逗号分隔
        if (!match(TokenType::Comma)) {
            break;
        }
    }

    // 期望 ]
    auto right_bracket = consume(TokenType::RightBracket, "Expected ']' after vector literal");
    if (!right_bracket.has_value()) [[unlikely]] {
        return std::unexpected(right_bracket.error());
    }

    return std::make_unique<ast::VectorExpression>(
        std::move(elements),
        ast_location(left_bracket.location())
    );
}

std::expected<std::string, ParserError> ParserWorker::parse_identifier_string(std::string_view message)
{
    // 期望标识符
    auto token = consume(TokenType::Identifier, message, ParserErrorCode::ExpectedIdentifier);
    if (!token.has_value()) [[unlikely]] {
        return std::unexpected(token.error());
    }

    return std::string(token->value());
}

std::expected<std::size_t, ParserError> ParserWorker::parse_integer_value(std::string_view message)
{
    // 期望整数字面量
    auto token = consume(TokenType::IntegerLiteral, message);
    if (!token.has_value()) [[unlikely]] {
        return std::unexpected(token.error());
    }

    std::size_t value = 0;
    const std::string_view text = token->value();
    const auto * begin = text.data();
    const auto * end = text.data() + text.size();
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc {} || result.ptr != end) [[unlikely]] {
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
        // 期望 (
        auto left_paren = consume(TokenType::LeftParen, "Expected '(' after VARCHAR");
        if (!left_paren.has_value()) [[unlikely]] {
            return std::unexpected(left_paren.error());
        }
        // 解析长度
        auto parameter = parse_integer_value("Expected VARCHAR length");
        if (!parameter.has_value()) [[unlikely]] {
            return std::unexpected(parameter.error());
        }
        // 期望 )
        auto right_paren = consume(TokenType::RightParen, "Expected ')' after VARCHAR length");
        if (!right_paren.has_value()) [[unlikely]] {
            return std::unexpected(right_paren.error());
        }
        return ast::DataType {ast::DataTypeKind::Varchar, parameter.value()};
    }
    if (match(TokenType::Vector)) {
        // 期望 (
        auto left_paren = consume(TokenType::LeftParen, "Expected '(' after VECTOR");
        if (!left_paren.has_value()) [[unlikely]] {
            return std::unexpected(left_paren.error());
        }
        // 解析维度
        auto parameter = parse_integer_value("Expected VECTOR dimension");
        if (!parameter.has_value()) [[unlikely]] {
            return std::unexpected(parameter.error());
        }
        // 期望 )
        auto right_paren = consume(TokenType::RightParen, "Expected ')' after VECTOR dimension");
        if (!right_paren.has_value()) [[unlikely]] {
            return std::unexpected(right_paren.error());
        }
        return ast::DataType {ast::DataTypeKind::Vector, parameter.value()};
    }

    return std::unexpected(make_current_error(ParserErrorCode::ExpectedDataType, "Expected data type"));
}

std::expected<ast::ColumnDefinition, ParserError> ParserWorker::parse_column_definition()
{
    // 解析列名称
    auto name = parse_identifier_string("Expected column name");
    if (!name.has_value()) [[unlikely]] {
        return std::unexpected(name.error());
    }

    // 解析数据类型
    auto type = parse_data_type();
    if (!type.has_value()) [[unlikely]] {
        return std::unexpected(type.error());
    }

    ast::ColumnDefinition column;
    column.name = std::move(name.value());
    column.type = type.value();

    // 循环解析列约束
    while (!check(TokenType::Comma) && !check(TokenType::RightParen) && !check(TokenType::EoF)) {
        // 尝试匹配 PRIMARY 关键字
        if (match(TokenType::Primary)) {
            // 期望 KEY 关键字
            auto key = consume(TokenType::Key, "Expected KEY after PRIMARY");
            if (!key.has_value()) [[unlikely]] {
                return std::unexpected(key.error());
            }
            column.primary_key = true;
        } else if (match(TokenType::Unique)) {
            column.unique = true;
        } else if (match(TokenType::Default)) {
            auto default_value = parse_literal_expression();
            if (!default_value.has_value()) [[unlikely]] {
                return std::unexpected(make_current_error(ParserErrorCode::ExpectedLiteral, "Expected literal after DEFAULT"));
            }
            column.default_value = std::move(default_value.value());
        } else if (match(TokenType::Comment)) {
            auto comment = consume(TokenType::StringLiteral, "Expected string literal after COMMENT");
            if (!comment.has_value()) [[unlikely]] {
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
    // 尝试匹配 DATABASE 或 DATABASES 关键字
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
    if (!not_token.has_value()) [[unlikely]] {
        return std::unexpected(not_token.error());
    }
    auto exists_token = consume(TokenType::Exists, "Expected EXISTS after IF NOT");
    if (!exists_token.has_value()) [[unlikely]] {
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
    if (!exists_token.has_value()) [[unlikely]] {
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
    return ParserWorker(*lexer_).parse();
}

ParserWorker::ParserWorker(Lexer & lexer)
    : lexer_(lexer)
    , current_token_(TokenType::EoF, "", TokenLocation {1, 1})
{
}

} // namespace litedb::core::parser
