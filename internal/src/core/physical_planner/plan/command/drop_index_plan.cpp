#include "core/physical_planner/plan/command/drop_index_plan.hpp"

namespace litedb::core::physical_planner::plan
{

DropIndexPlan::DropIndexPlan(std::optional<common::IndexId> index_id) noexcept
    : PhysicalPlan(PhysicalPlanKind::DropIndex)
    , index_id_(index_id)
{}

std::optional<common::IndexId> DropIndexPlan::index_id() const noexcept
{
    return index_id_;
}

} // namespace litedb::core::physical_planner::plan