#include "core/logical_planner/operator/logical_plan_operator.hpp"

namespace litedb::core::logical_planner::op
{

LogicalPlanOperator::LogicalPlanOperator(LogicalPlanOperatorKind kind) noexcept
    : kind_(kind)
{}

LogicalPlanOperatorKind LogicalPlanOperator::kind() const noexcept
{
    return kind_;
}

} // namespace litedb::core::logical_planner::op
