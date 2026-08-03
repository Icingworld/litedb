#include "core/logical_planner/worker/logical_planner_create_worker.hpp"

#include <memory>

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

std::expected<std::unique_ptr<plan::LogicalPlan>, LogicalPlannerError>
LogicalPlannerCreateWorker::plan_create_database(
    const BoundCreateDatabaseStatement & statement
)
{
    return std::make_unique<plan::CreateDatabasePlan>(
        statement.database_name()
    );
}

std::expected<std::unique_ptr<plan::LogicalPlan>, LogicalPlannerError>
LogicalPlannerCreateWorker::plan_create_collection(
    const BoundCreateCollectionStatement & statement
)
{
    return std::make_unique<plan::CreateCollectionPlan>(
        statement.database_id(),
        statement.collection_name(),
        statement.columns(),
        statement.comment()
    );
}

std::expected<std::unique_ptr<plan::LogicalPlan>, LogicalPlannerError>
LogicalPlannerCreateWorker::plan_create_index(
    const BoundCreateIndexStatement & statement
)
{
    return std::make_unique<plan::CreateIndexPlan>(
        statement.column_id(),
        statement.index_name(),
        statement.index_kind(),
        statement.unique()
    );
}

std::expected<std::unique_ptr<plan::LogicalPlan>, LogicalPlannerError>
LogicalPlannerCreateWorker::plan_create_vector_index(
    const BoundCreateVectorIndexStatement & statement
)
{
    return std::make_unique<plan::CreateVectorIndexPlan>(
        statement.column_id(),
        statement.vector_index_name(),
        statement.vector_index_kind(),
        statement.metric(),
        statement.max_neighbors(),
        statement.ef_construction(),
        statement.ef_search_default(),
        statement.random_seed()
    );
}

} // namespace litedb::core::logical_planner
