#include "core/parser/ast/expression/unary_expression.hpp"

#include <utility>
#include <cassert>

namespace litedb::core::parser::ast
{

UnaryExpression::UnaryExpression(
    TokenType op,
    std::unique_ptr<ExpressionNode> operand,
    AstNodeLocation location
)
    : ExpressionNode(location)
    , op_(op)
    , operand_(std::move(operand))
{
    assert(operand_ != nullptr);
}

AstNodeKind UnaryExpression::kind() const noexcept
{
    return AstNodeKind::Unary;
}

TokenType UnaryExpression::op() const noexcept
{
    return op_;
}

const ExpressionNode & UnaryExpression::operand() const noexcept
{
    return *operand_;
}

} // namespace litedb::core::parser::ast
