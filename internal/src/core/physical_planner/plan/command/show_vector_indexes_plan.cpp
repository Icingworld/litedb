#include "core/physical_planner/plan/command/show_vector_indexes_plan.hpp"

namespace litedb::core::physical_planner::plan
{

ShowVectorIndexesPlan::ShowVectorIndexesPlan(
    common::CollectionId collection_id
) noexcept
    : PhysicalPlan(PhysicalPlanKind::ShowVectorIndexes)
    , collection_id_(collection_id)
{
}

common::CollectionId
ShowVectorIndexesPlan::collection_id() const noexcept
{
    return collection_id_;
}

} // namespace litedb::core::physical_planner::plan