#include "core/physical_planner/worker/physical_query_worker.hpp"

#include "core/logical_planner/plan/query/query_plan.hpp"
#include "core/physical_planner/plan/query/query_plan.hpp"
#include "core/physical_planner/worker/physical_operator_worker.hpp"

namespace litedb::core::physical_planner
{

std::unique_ptr<plan::PhysicalPlan> PhysicalQueryWorker::plan_query(
    logical_planner::plan::QueryPlan & logical_plan
)
{
    return std::make_unique<plan::QueryPlan>(
        PhysicalOperatorWorker(context_).lower_operator(logical_plan.take_root_operator())
    );
}

} // namespace litedb::core::physical_planner
