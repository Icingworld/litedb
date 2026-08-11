#include "core/physical_planner/physical_planner_context.hpp"

namespace litedb::core::physical_planner
{

PhysicalPlannerContext::PhysicalPlannerContext(meta::CatalogView catalog) noexcept
    : catalog_(catalog)
{}

const meta::CatalogView & PhysicalPlannerContext::catalog() const noexcept
{
    return catalog_;
}

} // namespace litedb::core::physical_planner
