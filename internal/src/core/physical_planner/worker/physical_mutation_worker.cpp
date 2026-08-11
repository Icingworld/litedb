#include "core/physical_planner/worker/physical_mutation_worker.hpp"

#include "core/logical_planner/plan/mutation/delete_plan.hpp"
#include "core/logical_planner/plan/mutation/insert_plan.hpp"
#include "core/logical_planner/plan/mutation/update_plan.hpp"
#include "core/physical_planner/plan/mutation/delete_plan.hpp"
#include "core/physical_planner/plan/mutation/insert_plan.hpp"
#include "core/physical_planner/plan/mutation/update_plan.hpp"
#include "core/physical_planner/worker/physical_operator_worker.hpp"

namespace litedb::core::physical_planner
{

PhysicalMutationWorker::PhysicalMutationWorker(const PhysicalPlannerContext & context) noexcept
    : context_(context)
{
}

std::unique_ptr<plan::PhysicalPlan> PhysicalMutationWorker::plan_insert(
    logical_planner::plan::InsertPlan & logical_plan
)
{
    return std::make_unique<plan::InsertPlan>(
        logical_plan.collection_id(),
        logical_plan.take_values()
    );
}

std::unique_ptr<plan::PhysicalPlan> PhysicalMutationWorker::plan_update(
    logical_planner::plan::UpdatePlan & logical_plan
)
{
    auto collection_id = logical_plan.collection_id();
    auto assignments = logical_plan.take_assignments();
    auto root_operator = PhysicalOperatorWorker(context_).lower_operator(
        logical_plan.take_root_operator()
    );
    return std::make_unique<plan::UpdatePlan>(
        collection_id,
        std::move(assignments),
        std::move(root_operator)
    );
}

std::unique_ptr<plan::PhysicalPlan> PhysicalMutationWorker::plan_delete(
    logical_planner::plan::DeletePlan & logical_plan
)
{
    auto collection_id = logical_plan.collection_id();
    auto root_operator = PhysicalOperatorWorker(context_).lower_operator(
        logical_plan.take_root_operator()
    );
    return std::make_unique<plan::DeletePlan>(
        collection_id,
        std::move(root_operator)
    );
}

} // namespace litedb::core::physical_planner
