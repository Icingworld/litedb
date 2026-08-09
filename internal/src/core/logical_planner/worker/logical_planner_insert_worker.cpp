#include "core/logical_planner/worker/logical_planner_insert_worker.hpp"

#include "core/binder/bound/statement/bound_insert_statement.hpp"
#include "core/logical_planner/plan/mutation/insert_plan.hpp"

namespace litedb::core::logical_planner
{

using namespace litedb::core::binder::bound;

std::unique_ptr<plan::LogicalPlan> LogicalPlannerInsertWorker::plan_insert(
    BoundInsertStatement & statement
)
{
    return std::make_unique<plan::InsertPlan>(statement.collection_id(), statement.take_values());
}

} // namespace litedb::core::logical_planner
