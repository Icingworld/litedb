#include "core/binder/bound/expression/bound_between_expression.hpp"

#include <utility>

namespace litedb::core::binder::bound
{

BoundBetweenExpression::BoundBetweenExpression(
    std::unique_ptr<BoundExpression> expression,
    std::unique_ptr<BoundExpression> lower,
    std::unique_ptr<BoundExpression> upper
)
    : BoundExpression(
        BoundExpressionKind::Between,
        common::LogicalType {
            common::LogicalTypeId::Boolean,
            std::nullopt
        }
    )
    , expression_(std::move(expression))
    , lower_(std::move(lower))
    , upper_(std::move(upper))
{
}

const BoundExpression & BoundBetweenExpression::expression() const noexcept
{
    return *expression_;
}

std::unique_ptr<BoundExpression> BoundBetweenExpression::take_expression() noexcept
{
    return std::move(expression_);
}

const BoundExpression & BoundBetweenExpression::lower() const noexcept
{
    return *lower_;
}

std::unique_ptr<BoundExpression> BoundBetweenExpression::take_lower() noexcept
{
    return std::move(lower_);
}

const BoundExpression & BoundBetweenExpression::upper() const noexcept
{
    return *upper_;
}

std::unique_ptr<BoundExpression> BoundBetweenExpression::take_upper() noexcept
{
    return std::move(upper_);
}

} // namespace litedb::core::binder::bound
