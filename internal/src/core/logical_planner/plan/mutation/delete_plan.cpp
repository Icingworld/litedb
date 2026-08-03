#include "core/logical_planner/plan/mutation/delete_plan.hpp"

#include <utility>

namespace litedb::core::logical_planner::plan
{

DeletePlan::DeletePlan(
    common::CollectionId collection_id,
    std::unique_ptr<op::LogicalPlanOperator> root_operator
)
    : LogicalPlan(LogicalPlanKind::Delete)
    , collection_id_(collection_id)
    , root_operator_(std::move(root_operator))
{
}

const op::LogicalPlanOperator & DeletePlan::root_operator() const noexcept
{
    return *root_operator_;
}

common::CollectionId DeletePlan::collection_id() const noexcept
{
    return collection_id_;
}

} // namespace litedb::core::logical_planner::plan
