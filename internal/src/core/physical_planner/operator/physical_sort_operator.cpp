#include "core/physical_planner/operator/physical_sort_operator.hpp"

#include <utility>

namespace litedb::core::physical_planner::op
{

SortOperator::SortOperator(
    std::unique_ptr<PhysicalOperator> child,
    std::vector<binder::bound::BoundOrderByItem> order_by
) noexcept
    : PhysicalUnaryOperator(PhysicalOperatorKind::Sort, std::move(child))
    , order_by_(std::move(order_by))
{
}

const std::vector<binder::bound::BoundOrderByItem> &
SortOperator::order_by() const noexcept
{
    return order_by_;
}

} // namespace litedb::core::physical_planner::op
