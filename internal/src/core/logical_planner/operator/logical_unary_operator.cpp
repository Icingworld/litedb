#include "core/logical_planner/operator/logical_unary_operator.hpp"

#include <utility>

namespace litedb::core::logical_planner::op
{

LogicalUnaryOperator::LogicalUnaryOperator(
    LogicalPlanOperatorKind kind,
    std::unique_ptr<LogicalPlanOperator> child
) noexcept
    : LogicalPlanOperator(kind)
    , child_(std::move(child))
{
}

const LogicalPlanOperator & LogicalUnaryOperator::child() const noexcept
{
    return *child_;
}

} // namespace litedb::core::logical_planner::op
