#pragma once

#include <expected>
#include <memory>

#include "core/parser/parser_context.hpp"
#include "core/parser/parser_error.hpp"

namespace litedb::core::parser
{

namespace ast
{

class ExpressionNode;

} // namespace ast

/**
 * @brief 表达式解析工作器
 */
class ParserExpressionWorker
{
public:
    explicit ParserExpressionWorker(ParserContext & context);

public:
    /**
     * @brief 解析表达式
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError>
    parse_expression();

    /**
     * @brief 解析字面量表达式
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError>
    parse_literal_expression();

private:
    /**
     * @brief 解析 OR 表达式
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError>
    parse_or_expression();

    /**
     * @brief 解析 AND 表达式
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError>
    parse_and_expression();

    /**
     * @brief 解析 NOT 表达式
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError>
    parse_not_expression();

    /**
     * @brief 解析比较表达式
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError>
    parse_comparison_expression();

    /**
     * @brief 解析加减表达式
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError>
    parse_additive_expression();

    /**
     * @brief 解析乘除表达式
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError>
    parse_multiplicative_expression();

    /**
     * @brief 解析一元表达式
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError>
    parse_unary_expression();

    /**
     * @brief 解析主表达式
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError>
    parse_primary_expression();

    /**
     * @brief 解析列引用表达式
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError>
    parse_column_reference_expression();

    /**
     * @brief 解析函数调用或列引用
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError>
    parse_function_call_or_column_reference();

    /**
     * @brief 解析向量表达式
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError>
    parse_vector_expression();

private:
    ParserContext & context_;                   // 解析上下文
};

} // namespace litedb::core::parser
