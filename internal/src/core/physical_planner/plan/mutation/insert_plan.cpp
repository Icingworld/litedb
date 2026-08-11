#include "core/physical_planner/plan/mutation/insert_plan.hpp"

#include <utility>

namespace litedb::core::physical_planner::plan
{

InsertPlan::InsertPlan(
    common::CollectionId collection_id,
    std::vector<std::unique_ptr<binder::bound::BoundExpression>> values
)
    : PhysicalPlan(PhysicalPlanKind::Insert)
    , collection_id_(collection_id)
    , values_(std::move(values))
{}

common::CollectionId InsertPlan::collection_id() const noexcept
{
    return collection_id_;
}

const std::vector<std::unique_ptr<binder::bound::BoundExpression>> &
InsertPlan::values() const noexcept
{
    return values_;
}

} // namespace litedb::core::physical_planner::plan
