#include "core/executor/executor.hpp"

#include "core/executor/command_executor.hpp"
#include "core/executor/mutation_executor.hpp"
#include "core/executor/query_executor.hpp"

namespace litedb::core::executor
{

Executor::Executor(ExecutionContext context) noexcept
    : context_(context)
{
}

std::expected<ExecutionResult, ExecutionError> Executor::execute(
    const physical_planner::plan::DescribeCollectionPlan & plan
)
{
    return CommandExecutor {context_}.execute(plan);
}

std::expected<ExecutionResult, ExecutionError> Executor::execute(
    const physical_planner::plan::DeletePlan & plan
)
{
    return MutationExecutor {context_}.execute(plan);
}

std::expected<ExecutionResult, ExecutionError> Executor::execute(
    const physical_planner::plan::InsertPlan & plan
)
{
    return MutationExecutor {context_}.execute(plan);
}

std::expected<ExecutionResult, ExecutionError> Executor::execute(
    const physical_planner::plan::QueryPlan & plan
)
{
    return QueryExecutor {context_}.execute(plan);
}

std::expected<ExecutionResult, ExecutionError> Executor::execute(
    const physical_planner::plan::ShowCollectionsPlan & plan
)
{
    return CommandExecutor {context_}.execute(plan);
}

std::expected<ExecutionResult, ExecutionError> Executor::execute(
    const physical_planner::plan::ShowDatabasesPlan & plan
)
{
    return CommandExecutor {context_}.execute(plan);
}

std::expected<ExecutionResult, ExecutionError> Executor::execute(
    const physical_planner::plan::ShowIndexesPlan & plan
)
{
    return CommandExecutor {context_}.execute(plan);
}

std::expected<ExecutionResult, ExecutionError> Executor::execute(
    const physical_planner::plan::ShowVectorIndexesPlan & plan
)
{
    return CommandExecutor {context_}.execute(plan);
}

std::expected<ExecutionResult, ExecutionError> Executor::execute(
    const physical_planner::plan::UpdatePlan & plan
)
{
    return MutationExecutor {context_}.execute(plan);
}

std::expected<ExecutionResult, ExecutionError> Executor::execute(
    const physical_planner::plan::UsePlan & plan
)
{
    return CommandExecutor {context_}.execute(plan);
}

} // namespace litedb::core::executor
