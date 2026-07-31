#include "core/binder/bound/expression/bound_like_expression.hpp"

#include <utility>

namespace litedb::core::binder::bound
{

BoundLikeExpression::BoundLikeExpression(
    std::unique_ptr<BoundExpression> expression,
    std::unique_ptr<BoundExpression> pattern
)
    : BoundExpression(
        BoundExpressionKind::Like,
        common::LogicalType {
            common::LogicalTypeId::Boolean,
            std::nullopt
        }
    )
    , expression_(std::move(expression))
    , pattern_(std::move(pattern))
{
}

const BoundExpression & BoundLikeExpression::expression() const noexcept
{
    return *expression_;
}

const BoundExpression & BoundLikeExpression::pattern() const noexcept
{
    return *pattern_;
}

} // namespace litedb::core::binder::bound
