#include "core/physical_planner/plan/command/show_collections_plan.hpp"

namespace litedb::core::physical_planner::plan
{

ShowCollectionsPlan::ShowCollectionsPlan(
    common::DatabaseId database_id
) noexcept
    : PhysicalPlan(PhysicalPlanKind::ShowCollections)
    , database_id_(database_id)
{
}

common::DatabaseId ShowCollectionsPlan::database_id() const noexcept
{
    return database_id_;
}

} // namespace litedb::core::physical_planner::plan