#include "core/binder/bound/expression/bound_unary_expression.hpp"

#include <utility>

namespace litedb::core::binder::bound
{

BoundUnaryExpression::BoundUnaryExpression(
    parser::TokenType op,
    std::unique_ptr<BoundExpression> operand,
    common::LogicalType type,
    parser::ast::AstNodeLocation location
)
    : BoundExpression(BoundExpressionKind::Unary, type, location)
    , op_(op)
    , operand_(std::move(operand))
{
}

parser::TokenType BoundUnaryExpression::op() const noexcept
{
    return op_;
}

const BoundExpression & BoundUnaryExpression::operand() const noexcept
{
    return *operand_;
}

void BoundUnaryExpression::accept(BoundExpressionVisitor & visitor) const
{
    visitor.visit(*this);
}

} // namespace litedb::core::binder::bound
