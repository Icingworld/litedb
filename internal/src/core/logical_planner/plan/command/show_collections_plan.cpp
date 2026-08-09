#include "core/logical_planner/plan/command/show_collections_plan.hpp"

namespace litedb::core::logical_planner::plan
{

ShowCollectionsPlan::ShowCollectionsPlan(common::DatabaseId database_id) noexcept
    : LogicalPlan(LogicalPlanKind::ShowCollections)
    , database_id_(database_id)
{}

common::DatabaseId ShowCollectionsPlan::database_id() const noexcept
{
    return database_id_;
}

} // namespace litedb::core::logical_planner::plan
