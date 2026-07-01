#include "core/planner/plan/command/show_collections_plan.hpp"

namespace litedb::core::planner::plan
{

ShowCollectionsPlan::ShowCollectionsPlan(common::DatabaseId database_id, parser::ast::AstNodeLocation location)
    : StatementPlan(StatementPlanKind::ShowCollections, location)
    , database_id_(database_id)
{
}

common::DatabaseId ShowCollectionsPlan::database_id() const noexcept
{
    return database_id_;
}

} // namespace litedb::core::planner::plan
