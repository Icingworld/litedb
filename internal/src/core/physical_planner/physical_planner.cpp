#include "core/physical_planner/physical_planner.hpp"

#include <cassert>
#include <utility>

#include "core/physical_planner/physical_planner_context.hpp"
#include "core/physical_planner/worker/physical_planner_worker.hpp"

namespace litedb::core::physical_planner
{

PhysicalPlanner::PhysicalPlanner(meta::CatalogView catalog) noexcept
    : catalog_(catalog)
{
}

std::unique_ptr<plan::PhysicalPlan> PhysicalPlanner::plan(
    std::unique_ptr<logical_planner::plan::LogicalPlan> logical_plan
)
{
    assert(logical_plan != nullptr);
    PhysicalPlannerContext context {catalog_};
    return PhysicalPlannerWorker(context).plan_statement(std::move(logical_plan));
}

} // namespace litedb::core::physical_planner
