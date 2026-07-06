#include "core/binder/bound/expression/bound_unary_expression.hpp"

#include <memory>
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

std::unique_ptr<BoundExpression> BoundUnaryExpression::clone() const
{
    return std::make_unique<BoundUnaryExpression>(op_, operand_->clone(), type(), location());
}

} // namespace litedb::core::binder::bound
