#include "core/logical_planner/plan/command/use_plan.hpp"

namespace litedb::core::logical_planner::plan
{

UsePlan::UsePlan(
    common::DatabaseId database_id
) noexcept
    : LogicalPlan(LogicalPlanKind::Use)
    , database_id_(database_id)
{
}

common::DatabaseId UsePlan::database_id() const noexcept
{
    return database_id_;
}

} // namespace litedb::core::logical_planner::plan
