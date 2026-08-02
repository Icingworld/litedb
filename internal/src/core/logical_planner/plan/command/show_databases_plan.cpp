#include "core/logical_planner/plan/command/show_databases_plan.hpp"

namespace litedb::core::logical_planner::plan
{

ShowDatabasesPlan::ShowDatabasesPlan() noexcept
    : LogicalPlan(LogicalPlanKind::ShowDatabases)
{
}

} // namespace litedb::core::logical_planner::plan
