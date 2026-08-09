#include "core/logical_planner/worker/logical_planner_show_worker.hpp"

#include "core/binder/bound/statement/bound_show_collections_statement.hpp"
#include "core/binder/bound/statement/bound_show_databases_statement.hpp"
#include "core/binder/bound/statement/bound_show_indexes_statement.hpp"
#include "core/binder/bound/statement/bound_show_vector_indexes_statement.hpp"
#include "core/logical_planner/plan/command/show_collections_plan.hpp"
#include "core/logical_planner/plan/command/show_databases_plan.hpp"
#include "core/logical_planner/plan/command/show_indexes_plan.hpp"
#include "core/logical_planner/plan/command/show_vector_indexes_plan.hpp"

namespace litedb::core::logical_planner
{

using namespace litedb::core::binder::bound;

std::unique_ptr<plan::LogicalPlan> LogicalPlannerShowWorker::plan_show_databases(
    const BoundShowDatabasesStatement & /* statement */
)
{
    return std::make_unique<plan::ShowDatabasesPlan>();
}

std::unique_ptr<plan::LogicalPlan> LogicalPlannerShowWorker::plan_show_collections(
    const BoundShowCollectionsStatement & statement
)
{
    return std::make_unique<plan::ShowCollectionsPlan>(statement.database_id());
}

std::unique_ptr<plan::LogicalPlan> LogicalPlannerShowWorker::plan_show_indexes(
    const BoundShowIndexesStatement & statement
)
{
    return std::make_unique<plan::ShowIndexesPlan>(statement.collection_id());
}

std::unique_ptr<plan::LogicalPlan> LogicalPlannerShowWorker::plan_show_vector_indexes(
    const BoundShowVectorIndexesStatement & statement
)
{
    return std::make_unique<plan::ShowVectorIndexesPlan>(statement.collection_id());
}

} // namespace litedb::core::logical_planner
