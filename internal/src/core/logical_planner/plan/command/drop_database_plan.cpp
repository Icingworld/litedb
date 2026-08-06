#include "core/logical_planner/plan/command/drop_database_plan.hpp"

namespace litedb::core::logical_planner::plan
{

DropDatabasePlan::DropDatabasePlan(
    std::optional<common::DatabaseId> database_id
) noexcept
    : LogicalPlan(LogicalPlanKind::DropDatabase)
    , database_id_(database_id)
{
}

std::optional<common::DatabaseId>
DropDatabasePlan::database_id() const noexcept
{
    return database_id_;
}

} // namespace litedb::core::logical_planner::plan
