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

// 表达式解析工作器
// 手写递归下降解析表达式，共用一份上下文
// 在种类不多的情况下，放在同一个类中，比拆分成多个类更清晰
class ParserExpressionWorker
{
public:
    explicit ParserExpressionWorker(ParserContext & context) noexcept;

public:
    // 解析表达式
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError>
    parse_expression();

    // 解析字面量表达式
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError>
    parse_literal_expression();

private:
    // 解析 OR 表达式
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError>
    parse_or_expression();

    // 解析 AND 表达式
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError>
    parse_and_expression();

    // 解析 NOT 表达式
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError>
    parse_not_expression();

    // 解析比较表达式
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError>
    parse_comparison_expression();

    // 解析加减表达式
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError>
    parse_additive_expression();

    // 解析乘除表达式
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError>
    parse_multiplicative_expression();

    // 解析一元表达式
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError>
    parse_unary_expression();

    // 解析主表达式
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError>
    parse_primary_expression();

    // 解析列引用表达式
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError>
    parse_column_reference_expression();

    // 解析函数调用
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError>
    parse_function_call_expression();

    // 解析向量表达式
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError>
    parse_vector_expression();

private:
    ParserContext & context_;                   // 解析上下文
};

} // namespace litedb::core::parser
