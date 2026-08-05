#include "core/physical_planner/plan/command/show_databases_plan.hpp"

namespace litedb::core::physical_planner::plan
{

ShowDatabasesPlan::ShowDatabasesPlan() noexcept
    : PhysicalPlan(PhysicalPlanKind::ShowDatabases)
{
}

} // namespace litedb::core::physical_planner::plan