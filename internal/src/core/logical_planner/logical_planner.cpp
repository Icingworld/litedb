#include "core/logical_planner/logical_planner.hpp"

#include <cassert>

#include "core/logical_planner/worker/logical_planner_worker.hpp"
#include "core/logical_planner/plan/logical_plan.hpp"

namespace litedb::core::logical_planner
{

std::unique_ptr<plan::LogicalPlan>
LogicalPlanner::plan(
    std::unique_ptr<binder::bound::BoundStatement> statement
) const
{
    assert(statement != nullptr);
    return LogicalPlannerWorker().plan_statement(*statement);
}

} // namespace litedb::core::logical_planner
