#include "core/logical_planner/worker/logical_planner_create_worker.hpp"

#include "core/binder/bound/statement/bound_create_collection_statement.hpp"
#include "core/binder/bound/statement/bound_create_database_statement.hpp"
#include "core/binder/bound/statement/bound_create_index_statement.hpp"
#include "core/binder/bound/statement/bound_create_vector_index_statement.hpp"
#include "core/logical_planner/plan/command/create_collection_plan.hpp"
#include "core/logical_planner/plan/command/create_database_plan.hpp"
#include "core/logical_planner/plan/command/create_index_plan.hpp"
#include "core/logical_planner/plan/command/create_vector_index_plan.hpp"

namespace litedb::core::logical_planner
{

using namespace litedb::core::binder::bound;

std::unique_ptr<plan::LogicalPlan> LogicalPlannerCreateWorker::plan_create_database(
    BoundCreateDatabaseStatement & statement
)
{
    return std::make_unique<plan::CreateDatabasePlan>(statement.take_database_name());
}

std::unique_ptr<plan::LogicalPlan> LogicalPlannerCreateWorker::plan_create_collection(
    BoundCreateCollectionStatement & statement
)
{
    return std::make_unique<plan::CreateCollectionPlan>(
        statement.database_id(),
        statement.take_collection_name(),
        statement.take_columns(),
        statement.take_comment()
    );
}

std::unique_ptr<plan::LogicalPlan> LogicalPlannerCreateWorker::plan_create_index(
    BoundCreateIndexStatement & statement
)
{
    return std::make_unique<plan::CreateIndexPlan>(
        statement.column_id(),
        statement.take_index_name(),
        statement.index_kind(),
        statement.unique()
    );
}

std::unique_ptr<plan::LogicalPlan> LogicalPlannerCreateWorker::plan_create_vector_index(
    BoundCreateVectorIndexStatement & statement
)
{
    return std::make_unique<plan::CreateVectorIndexPlan>(
        statement.column_id(),
        statement.take_vector_index_name(),
        statement.vector_index_kind(),
        statement.metric(),
        statement.max_neighbors(),
        statement.ef_construction(),
        statement.ef_search_default(),
        statement.random_seed()
    );
}

} // namespace litedb::core::logical_planner
