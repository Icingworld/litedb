#include "core/executor/command_executor.hpp"

#include "core/executor/executor_detail.hpp"

namespace litedb::core::executor
{

CommandExecutor::CommandExecutor(ExecutionContext & context) noexcept
    : context_(context)
{
}

std::expected<ExecutionResult, ExecutionError> CommandExecutor::execute(
    const physical_planner::plan::DescribeCollectionPlan & plan
)
{
    return detail::execute_describe_collection(plan, context_.catalog);
}

std::expected<ExecutionResult, ExecutionError> CommandExecutor::execute(
    const physical_planner::plan::ShowCollectionsPlan & plan
)
{
    return detail::execute_show_collections(plan, context_.catalog);
}

std::expected<ExecutionResult, ExecutionError> CommandExecutor::execute(
    const physical_planner::plan::ShowDatabasesPlan &
)
{
    return detail::execute_show_databases(context_.catalog);
}

std::expected<ExecutionResult, ExecutionError> CommandExecutor::execute(
    const physical_planner::plan::ShowIndexesPlan & plan
)
{
    return detail::execute_show_indexes(plan, context_.catalog);
}

std::expected<ExecutionResult, ExecutionError> CommandExecutor::execute(
    const physical_planner::plan::ShowVectorIndexesPlan & plan
)
{
    return detail::execute_show_vector_indexes(plan, context_.catalog);
}

std::expected<ExecutionResult, ExecutionError> CommandExecutor::execute(
    const physical_planner::plan::UsePlan & plan
)
{
    return detail::execute_use(plan, context_.catalog);
}

} // namespace litedb::core::executor
