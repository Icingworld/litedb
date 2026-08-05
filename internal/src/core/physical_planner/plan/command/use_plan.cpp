#include "core/physical_planner/plan/command/use_plan.hpp"

namespace litedb::core::physical_planner::plan
{

UsePlan::UsePlan(common::DatabaseId database_id) noexcept
    : PhysicalPlan(PhysicalPlanKind::Use)
    , database_id_(database_id)
{
}

common::DatabaseId UsePlan::database_id() const noexcept
{
    return database_id_;
}

} // namespace litedb::core::physical_planner::plan