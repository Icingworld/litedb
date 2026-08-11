#include "core/executor/executor.hpp"

#include <string>
#include <string_view>

#include "core/executor/command_executor.hpp"
#include "core/executor/mutation_executor.hpp"
#include "core/executor/query_executor.hpp"

namespace litedb::core::executor
{

namespace
{

[[nodiscard]]
std::expected<ExecutionResult, ExecutionError> unsupported_by_executor(std::string_view message)
{
    return std::unexpected(
        ExecutionError {
            ExecutionErrorCode::UnsupportedStatement,
            std::string(message),
        }
    );
}

} // namespace

Executor::Executor(ExecutionContext context) noexcept
    : context_(context)
{}

Executor::Result Executor::execute(const physical_planner::plan::PhysicalPlan & plan)
{
    return dispatch_plan(plan);
}

Executor::Result Executor::visit_use_plan(const physical_planner::plan::UsePlan & plan)
{
    return CommandExecutor {context_}.execute(plan);
}

Executor::Result Executor::visit_create_database_plan(
    const physical_planner::plan::CreateDatabasePlan &
)
{
    return unsupported_by_executor("CREATE DATABASE is not handled by Executor");
}

Executor::Result Executor::visit_create_collection_plan(
    const physical_planner::plan::CreateCollectionPlan &
)
{
    return unsupported_by_executor("CREATE COLLECTION is not handled by Executor");
}

Executor::Result Executor::visit_create_index_plan(const physical_planner::plan::CreateIndexPlan &)
{
    return unsupported_by_executor("CREATE INDEX is not handled by Executor");
}

Executor::Result Executor::visit_create_vector_index_plan(
    const physical_planner::plan::CreateVectorIndexPlan &
)
{
    return unsupported_by_executor("CREATE VINDEX is not handled by Executor");
}

Executor::Result Executor::visit_drop_database_plan(
    const physical_planner::plan::DropDatabasePlan &
)
{
    return unsupported_by_executor("DROP DATABASE is not handled by Executor");
}

Executor::Result Executor::visit_drop_collection_plan(
    const physical_planner::plan::DropCollectionPlan &
)
{
    return unsupported_by_executor("DROP COLLECTION is not handled by Executor");
}

Executor::Result Executor::visit_drop_index_plan(const physical_planner::plan::DropIndexPlan &)
{
    return unsupported_by_executor("DROP INDEX is not handled by Executor");
}

Executor::Result Executor::visit_drop_vector_index_plan(
    const physical_planner::plan::DropVectorIndexPlan &
)
{
    return unsupported_by_executor("DROP VINDEX is not handled by Executor");
}

Executor::Result Executor::visit_show_databases_plan(
    const physical_planner::plan::ShowDatabasesPlan & plan
)
{
    return CommandExecutor {context_}.execute(plan);
}

Executor::Result Executor::visit_show_collections_plan(
    const physical_planner::plan::ShowCollectionsPlan & plan
)
{
    return CommandExecutor {context_}.execute(plan);
}

Executor::Result Executor::visit_show_indexes_plan(
    const physical_planner::plan::ShowIndexesPlan & plan
)
{
    return CommandExecutor {context_}.execute(plan);
}

Executor::Result Executor::visit_show_vector_indexes_plan(
    const physical_planner::plan::ShowVectorIndexesPlan & plan
)
{
    return CommandExecutor {context_}.execute(plan);
}

Executor::Result Executor::visit_describe_collection_plan(
    const physical_planner::plan::DescribeCollectionPlan & plan
)
{
    return CommandExecutor {context_}.execute(plan);
}

Executor::Result Executor::visit_insert_plan(const physical_planner::plan::InsertPlan & plan)
{
    return MutationExecutor {context_}.execute(plan);
}

Executor::Result Executor::visit_update_plan(const physical_planner::plan::UpdatePlan & plan)
{
    return MutationExecutor {context_}.execute(plan);
}

Executor::Result Executor::visit_delete_plan(const physical_planner::plan::DeletePlan & plan)
{
    return MutationExecutor {context_}.execute(plan);
}

Executor::Result Executor::visit_query_plan(const physical_planner::plan::QueryPlan & plan)
{
    return QueryExecutor {context_}.execute(plan);
}

} // namespace litedb::core::executor
