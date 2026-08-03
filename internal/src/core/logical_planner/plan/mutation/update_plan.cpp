#include "core/logical_planner/plan/mutation/update_plan.hpp"

#include <utility>

namespace litedb::core::logical_planner::plan
{

UpdatePlan::UpdatePlan(
    common::CollectionId collection_id,
    std::vector<binder::bound::BoundAssignment> assignments,
    std::unique_ptr<op::LogicalPlanOperator> root_operator
)
    : LogicalPlan(LogicalPlanKind::Update)
    , collection_id_(collection_id)
    , assignments_(std::move(assignments))
    , root_operator_(std::move(root_operator))
{
}

const op::LogicalPlanOperator & UpdatePlan::root_operator() const noexcept
{
    return *root_operator_;
}

common::CollectionId UpdatePlan::collection_id() const noexcept
{
    return collection_id_;
}

const std::vector<binder::bound::BoundAssignment> &
UpdatePlan::assignments() const noexcept
{
    return assignments_;
}

} // namespace litedb::core::logical_planner::plan
