#include "core/physical_planner/plan/command/drop_vector_index_plan.hpp"

namespace litedb::core::physical_planner::plan
{

DropVectorIndexPlan::DropVectorIndexPlan(std::optional<common::VIndexId> index_id) noexcept
    : PhysicalPlan(PhysicalPlanKind::DropVectorIndex)
    , index_id_(index_id)
{}

std::optional<common::VIndexId> DropVectorIndexPlan::index_id() const noexcept
{
    return index_id_;
}

} // namespace litedb::core::physical_planner::plan