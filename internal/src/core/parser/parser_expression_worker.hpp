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
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> parse_expression();

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

private:
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
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> parse_function_call_or_column_reference();

    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> parse_vector_expression();

private:
    ParserContext & context_;   ///< 解析上下文
};

} // namespace litedb::core::parser
