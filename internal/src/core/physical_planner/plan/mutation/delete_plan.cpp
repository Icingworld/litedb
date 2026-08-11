#include "core/physical_planner/plan/mutation/delete_plan.hpp"

#include <utility>

namespace litedb::core::physical_planner::plan
{

DeletePlan::DeletePlan(
    common::CollectionId collection_id,
    std::unique_ptr<op::PhysicalOperator> root_operator
)
    : PhysicalPlan(PhysicalPlanKind::Delete)
    , collection_id_(collection_id)
    , root_operator_(std::move(root_operator))
{}

common::CollectionId DeletePlan::collection_id() const noexcept
{
    return collection_id_;
}

const op::PhysicalOperator & DeletePlan::root_operator() const noexcept
{
    return *root_operator_;
}

} // namespace litedb::core::physical_planner::plan
