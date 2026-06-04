#pragma once

#include <memory>
#include <expected>
#include <string>
#include <string_view>

#include "core/parser/ast/ast_node.hpp"
#include "core/parser/ast/schema.hpp"
#include "core/parser/token.hpp"

namespace litedb::core::parser
{

class Lexer;

namespace ast
{

class StatementNode;
class ExpressionNode;

} // namespace ast

/**
 * @brief 语法解析错误类型
 */
struct ParserError
{
    TokenLocation location;      ///< 错误位置
    std::string message;         ///< 错误信息
};

/**
 * @brief SQL 语法解析器
 */
class Parser
{
public:
    explicit Parser(std::string input);

    explicit Parser(std::unique_ptr<Lexer> lexer);

    Parser(const Parser &) = delete;

    Parser & operator=(const Parser &) = delete;

    Parser(Parser &&) noexcept = default;

    Parser & operator=(Parser &&) noexcept = default;

    ~Parser();

public:
    /**
     * @brief 解析 SQL 语句
     * @return 解析结果，如果解析失败，返回错误类型
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError> parse();

private:
    // 语句解析

    /**
     * @brief 解析语句
     * @return 解析结果，如果解析失败，返回错误类型
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError> parse_statement();

    /**
     * @brief 解析 USE 语句
     * @return 解析结果，如果解析失败，返回错误类型
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError> parse_use_statement();

    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError> parse_create_statement();

    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError> parse_drop_statement();

    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError> parse_show_statement();

    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError> parse_describe_statement();

    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError> parse_insert_statement();

    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError> parse_update_statement();

    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError> parse_delete_statement();

    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError> parse_select_statement();

    // 表达式解析

    /**
     * @brief 解析表达式
     * @return 解析结果，如果解析失败，返回错误类型
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> parse_expression();

    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> parse_or_expression();

    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> parse_and_expression();

    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> parse_not_expression();

    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> parse_comparison_expression();

    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> parse_additive_expression();

    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> parse_multiplicative_expression();

    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> parse_unary_expression();

    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> parse_primary_expression();

    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> parse_column_reference_expression();

    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> parse_wildcard_or_column_reference();

    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> parse_literal_expression();

    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> parse_vector_expression();

    [[nodiscard]]
    std::expected<std::string, ParserError> parse_identifier_string(std::string_view message);

    [[nodiscard]]
    std::expected<std::size_t, ParserError> parse_integer_value(std::string_view message);

    [[nodiscard]]
    std::expected<ast::DataType, ParserError> parse_data_type();

    [[nodiscard]]
    std::expected<ast::ColumnDefinition, ParserError> parse_column_definition();

    [[nodiscard]]
    std::expected<ast::SchemaObjectType, ParserError> parse_schema_object_type(bool plural);

    [[nodiscard]]
    std::expected<bool, ParserError> parse_if_not_exists();

    [[nodiscard]]
    std::expected<bool, ParserError> parse_if_exists();

    /**
     * @brief 获取下一个 Token
     */
    Token advance();

    /**
     * @brief 检查当前 Token 类型，匹配成功则消耗
     */
    bool match(TokenType type);
 
    /**
     * @brief 检查当前 Token 类型（不消耗）
     */
    bool check(TokenType type) const;
 
    /**
     * @brief 消耗当前 Token，如果类型不匹配则返回错误信息
     * @param type 期望的 Token 类型
     * @return 如果消耗成功，返回被消耗的 Token，如果消耗失败，返回错误信息
     */
    [[nodiscard]]
    std::expected<Token, ParserError> consume(TokenType type, std::string_view message);
 
    /**
     * @brief 跳过分号（如果存在）
     */
    void skip_semicolon();

    [[nodiscard]]
    ast::AstNodeLocation ast_location(TokenLocation location) const noexcept;

private:
    std::unique_ptr<Lexer> lexer_;      ///< 词法分析器
    Token current_token_;               ///< 当前词法单元
};

} // namespace litedb::core::parser
