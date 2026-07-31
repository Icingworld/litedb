#include "core/binder/bound/expression/bound_cast_expression.hpp"

#include <utility>

namespace litedb::core::binder::bound
{

BoundCastExpression::BoundCastExpression(
    std::unique_ptr<BoundExpression> expression,
    common::LogicalType target_type
)
    : BoundExpression(BoundExpressionKind::Cast, target_type)
    , expression_(std::move(expression))
{
}

const BoundExpression & BoundCastExpression::expression() const noexcept
{
    return *expression_;
}

} // namespace litedb::core::binder::bound
