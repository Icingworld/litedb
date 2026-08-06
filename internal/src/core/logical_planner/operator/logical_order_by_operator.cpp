#include "core/logical_planner/operator/logical_order_by_operator.hpp"

#include <utility>

namespace litedb::core::logical_planner::op
{

LogicalOrderByOperator::LogicalOrderByOperator(
    std::unique_ptr<LogicalPlanOperator> child,
    std::vector<binder::bound::BoundOrderByItem> order_by
)
    : LogicalUnaryOperator(
        LogicalPlanOperatorKind::OrderBy,
        std::move(child)
    )
    , order_by_(std::move(order_by))
{
}

const std::vector<binder::bound::BoundOrderByItem> &
LogicalOrderByOperator::order_by() const noexcept
{
    return order_by_;
}

std::vector<binder::bound::BoundOrderByItem>
LogicalOrderByOperator::take_order_by() noexcept
{
    return std::move(order_by_);
}

} // namespace litedb::core::logical_planner::op
