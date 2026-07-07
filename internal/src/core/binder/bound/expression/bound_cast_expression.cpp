#include "core/binder/bound/expression/bound_cast_expression.hpp"

#include <memory>
#include <utility>

namespace litedb::core::binder::bound
{

BoundCastExpression::BoundCastExpression(
    std::unique_ptr<BoundExpression> expression,
    common::LogicalType target_type,
    parser::ast::AstNodeLocation location
)
    : BoundExpression(BoundExpressionKind::Cast, target_type, location)
    , expression_(std::move(expression))
{
}

const BoundExpression & BoundCastExpression::expression() const noexcept
{
    return *expression_;
}

void BoundCastExpression::accept(BoundExpressionVisitor & visitor) const
{
    visitor.visit(*this);
}

std::unique_ptr<BoundExpression> BoundCastExpression::clone() const
{
    return std::make_unique<BoundCastExpression>(expression_->clone(), type(), location());
}

} // namespace litedb::core::binder::bound
