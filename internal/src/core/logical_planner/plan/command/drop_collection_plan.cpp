#include "core/logical_planner/plan/command/drop_collection_plan.hpp"

namespace litedb::core::logical_planner::plan
{

DropCollectionPlan::DropCollectionPlan(std::optional<common::CollectionId> collection_id) noexcept
    : LogicalPlan(LogicalPlanKind::DropCollection)
    , collection_id_(collection_id)
{}

std::optional<common::CollectionId> DropCollectionPlan::collection_id() const noexcept
{
    return collection_id_;
}

} // namespace litedb::core::logical_planner::plan
