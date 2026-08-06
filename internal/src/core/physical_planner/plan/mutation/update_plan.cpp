#include "core/physical_planner/plan/mutation/update_plan.hpp"

#include <utility>

namespace litedb::core::physical_planner::plan
{

UpdatePlan::UpdatePlan(
    common::CollectionId collection_id,
    std::vector<binder::bound::BoundAssignment> assignments,
    std::unique_ptr<op::PhysicalOperator> root_operator
)
    : PhysicalPlan(PhysicalPlanKind::Update)
    , collection_id_(collection_id)
    , assignments_(std::move(assignments))
    , root_operator_(std::move(root_operator))
{
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

const op::PhysicalOperator & UpdatePlan::root_operator() const noexcept
{
    return *root_operator_;
}

} // namespace litedb::core::physical_planner::plan
