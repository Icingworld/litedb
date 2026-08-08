#pragma once

#include <cstddef>
#include <expected>
#include <memory>

#include "core/parser/ast/expression/expression_node.hpp"
#include "core/parser/parser_context.hpp"
#include "core/parser/parser_error.hpp"

namespace litedb::core::parser
{

// 解析到的表达式及其嵌套深度
struct ParsedExpression
{
    std::unique_ptr<ast::ExpressionNode> expression; // 解析到的表达式
    std::size_t depth; // 嵌套深度
};

// 表达式解析工作器
class ParserExpressionWorker
{
public:
    explicit ParserExpressionWorker(ParserContext & context) noexcept;

public:
    // 解析表达式
    [[nodiscard]]
    std::expected<ParsedExpression, ParserError> parse_expression();

    // 解析字面量表达式
    [[nodiscard]]
    std::expected<ParsedExpression, ParserError> parse_literal_expression();

private:
    // 解析嵌套表达式
    [[nodiscard]]
    std::expected<ParsedExpression, ParserError> parse_nested_expression(TokenLocation location);

    // 解析表达式优先级
    [[nodiscard]]
    std::expected<ParsedExpression, ParserError> parse_expression_precedence(
        int minimum_precedence,
        bool allow_not_prefix,
        bool * comparison_consumed_out = nullptr
    );

    // 解析主表达式
    [[nodiscard]]
    std::expected<ParsedExpression, ParserError> parse_primary_expression();

    // 解析列引用表达式
    [[nodiscard]]
    std::expected<ParsedExpression, ParserError> parse_column_reference_expression();

    // 解析函数调用表达式
    [[nodiscard]]
    std::expected<ParsedExpression, ParserError> parse_function_call_expression();

    // 解析向量表达式
    [[nodiscard]]
    std::expected<ParsedExpression, ParserError> parse_vector_expression();

    // 创建一元表达式
    [[nodiscard]]
    std::expected<ParsedExpression, ParserError>
    make_unary(ParsedExpression operand, TokenType op, TokenLocation location) const;

    // 创建二元表达式
    [[nodiscard]]
    std::expected<ParsedExpression, ParserError> make_binary(
        ParsedExpression left,
        TokenType op,
        ParsedExpression right,
        TokenLocation location
    ) const;

private:
    ParserContext & context_;
};

} // namespace litedb::core::parser
