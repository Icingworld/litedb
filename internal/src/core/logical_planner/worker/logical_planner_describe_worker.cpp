#include "core/logical_planner/worker/logical_planner_describe_worker.hpp"

#include "core/binder/bound/statement/bound_describe_collection_statement.hpp"
#include "core/logical_planner/plan/command/describe_collection_plan.hpp"

namespace litedb::core::logical_planner
{

using namespace litedb::core::binder::bound;

std::unique_ptr<plan::LogicalPlan> LogicalPlannerDescribeWorker::plan_describe_collection(
    const BoundDescribeCollectionStatement & statement
)
{
    return std::make_unique<plan::DescribeCollectionPlan>(statement.collection_id());
}

} // namespace litedb::core::logical_planner
