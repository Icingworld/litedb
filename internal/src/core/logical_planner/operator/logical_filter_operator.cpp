#include "core/logical_planner/operator/logical_filter_operator.hpp"

#include <utility>

namespace litedb::core::logical_planner::op
{

LogicalFilterOperator::LogicalFilterOperator(
    std::unique_ptr<LogicalPlanOperator> child,
    std::unique_ptr<binder::bound::BoundExpression> predicate
)
    : LogicalUnaryOperator(
        LogicalPlanOperatorKind::Filter,
        std::move(child)
    )
    , predicate_(std::move(predicate))
{
}

const binder::bound::BoundExpression &
LogicalFilterOperator::predicate() const noexcept
{
    return *predicate_;
}

} // namespace litedb::core::logical_planner::op
