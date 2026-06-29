#include "core/binder/bound/expression/bound_cast_expression.hpp"

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

} // namespace litedb::core::binder::bound
