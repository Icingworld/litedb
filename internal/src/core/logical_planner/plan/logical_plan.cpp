#include "core/logical_planner/plan/logical_plan.hpp"

namespace litedb::core::logical_planner::plan
{

LogicalPlan::LogicalPlan(LogicalPlanKind kind) noexcept
    : kind_(kind)
{}

LogicalPlanKind LogicalPlan::kind() const noexcept
{
    return kind_;
}

} // namespace litedb::core::logical_planner::plan
