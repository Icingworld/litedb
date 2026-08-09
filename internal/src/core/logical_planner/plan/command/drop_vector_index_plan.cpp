#include "core/logical_planner/plan/command/drop_vector_index_plan.hpp"

namespace litedb::core::logical_planner::plan
{

DropVectorIndexPlan::DropVectorIndexPlan(std::optional<common::VIndexId> vector_index_id) noexcept
    : LogicalPlan(LogicalPlanKind::DropVectorIndex)
    , vector_index_id_(vector_index_id)
{}

std::optional<common::VIndexId> DropVectorIndexPlan::vector_index_id() const noexcept
{
    return vector_index_id_;
}

} // namespace litedb::core::logical_planner::plan
