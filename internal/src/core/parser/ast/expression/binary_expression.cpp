#include "core/parser/ast/expression/binary_expression.hpp"

#include <utility>

namespace litedb::core::parser::ast
{

BinaryExpression::BinaryExpression(
    std::unique_ptr<ExpressionNode> left,
    TokenType op,
    std::unique_ptr<ExpressionNode> right,
    AstNodeLocation location
) noexcept
    : ExpressionNode(location)
    , left_(std::move(left))
    , op_(op)
    , right_(std::move(right))
{
}

AstNodeKind BinaryExpression::kind() const noexcept
{
    return AstNodeKind::Binary;
}

const ExpressionNode & BinaryExpression::left() const noexcept
{
    return *left_;
}

TokenType BinaryExpression::op() const noexcept
{
    return op_;
}

const ExpressionNode & BinaryExpression::right() const noexcept
{
    return *right_;
}

} // namespace litedb::core::parser::ast
