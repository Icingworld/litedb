#include "core/physical_planner/plan/command/show_indexes_plan.hpp"

namespace litedb::core::physical_planner::plan
{

ShowIndexesPlan::ShowIndexesPlan(common::CollectionId collection_id) noexcept
    : PhysicalPlan(PhysicalPlanKind::ShowIndexes)
    , collection_id_(collection_id)
{}

common::CollectionId ShowIndexesPlan::collection_id() const noexcept
{
    return collection_id_;
}

} // namespace litedb::core::physical_planner::plan