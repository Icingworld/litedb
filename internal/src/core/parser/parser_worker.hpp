#pragma once

#include <cstddef>
#include <expected>
#include <memory>
#include <string>
#include <string_view>

#include "core/parser/ast/ast_node.hpp"
#include "core/parser/ast/schema.hpp"
#include "core/parser/lexer.hpp"
#include "core/parser/parser_error.hpp"

namespace litedb::core::parser
{

namespace ast
{

class StatementNode;
class ExpressionNode;

} // namespace ast

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
     * @brief 解析 CREATE VINDEX 语句
     * @param location CREATE 位置
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError> parse_create_vector_index_statement(
        TokenLocation location
    );

    /**
     * @brief 解析 DROP 语句
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError> parse_drop_statement();

    /**
     * @brief 解析 DROP VINDEX 语句
     * @param location DROP 位置
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError> parse_drop_vector_index_statement(
        TokenLocation location
    );

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
     * @brief 瑙ｆ瀽鍑芥暟璋冪敤鎴栧垪寮曠敤琛ㄨ揪寮?
     * @return 瑙ｆ瀽缁撴灉
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> parse_function_call_or_column_reference();

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

} // namespace litedb::core::parser
