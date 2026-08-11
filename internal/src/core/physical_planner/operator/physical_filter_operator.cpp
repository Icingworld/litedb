#include "core/physical_planner/operator/physical_filter_operator.hpp"

#include <utility>
#include <cassert>

namespace litedb::core::physical_planner::op
{

FilterOperator::FilterOperator(
    std::unique_ptr<PhysicalOperator> child,
    std::unique_ptr<binder::bound::BoundExpression> predicate
) noexcept
    : PhysicalUnaryOperator(PhysicalOperatorKind::Filter, std::move(child))
    , predicate_(std::move(predicate))
{
    assert(predicate_ != nullptr);
}

const binder::bound::BoundExpression &
FilterOperator::predicate() const noexcept
{
    return *predicate_;
}

} // namespace litedb::core::physical_planner::op
