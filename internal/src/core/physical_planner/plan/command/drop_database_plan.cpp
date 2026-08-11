#include "core/physical_planner/plan/command/drop_database_plan.hpp"

namespace litedb::core::physical_planner::plan
{

DropDatabasePlan::DropDatabasePlan(std::optional<common::DatabaseId> database_id) noexcept
    : PhysicalPlan(PhysicalPlanKind::DropDatabase)
    , database_id_(database_id)
{}

std::optional<common::DatabaseId> DropDatabasePlan::database_id() const noexcept
{
    return database_id_;
}

} // namespace litedb::core::physical_planner::plan