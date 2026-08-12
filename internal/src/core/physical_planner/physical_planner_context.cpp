#include "core/physical_planner/physical_planner_context.hpp"

namespace litedb::core::physical_planner
{

PhysicalPlannerContext::PhysicalPlannerContext(catalog::CatalogViewer catalog) noexcept
    : catalog_(catalog)
{}

const catalog::CatalogViewer & PhysicalPlannerContext::catalog() const noexcept
{
    return catalog_;
}

} // namespace litedb::core::physical_planner
