#include "core/binder/bound/expression/bound_in_expression.hpp"

#include <utility>

namespace litedb::core::binder::bound
{

BoundInExpression::BoundInExpression(
    std::unique_ptr<BoundExpression> expression,
    std::vector<std::unique_ptr<BoundExpression>> values
)
    : BoundExpression(
        BoundExpressionKind::In,
        common::LogicalType {
            common::LogicalTypeId::Boolean,
            std::nullopt
        }
    )
    , expression_(std::move(expression))
    , values_(std::move(values))
{
}

const BoundExpression & BoundInExpression::expression() const noexcept
{
    return *expression_;
}

const std::vector<std::unique_ptr<BoundExpression>> & BoundInExpression::values() const noexcept
{
    return values_;
}

} // namespace litedb::core::binder::bound
