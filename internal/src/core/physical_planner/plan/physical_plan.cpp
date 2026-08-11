#include "core/physical_planner/plan/physical_plan.hpp"

namespace litedb::core::physical_planner::plan
{

PhysicalPlan::PhysicalPlan(PhysicalPlanKind kind) noexcept
    : kind_(kind)
{}

PhysicalPlanKind PhysicalPlan::kind() const noexcept
{
    return kind_;
}

} // namespace litedb::core::physical_planner::plan
