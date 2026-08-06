#include "core/executor/mutation_executor.hpp"

#include "core/executor/executor_detail.hpp"

namespace litedb::core::executor
{

MutationExecutor::MutationExecutor(ExecutionContext & context) noexcept
    : context_(context)
{
}

std::expected<ExecutionResult, ExecutionError> MutationExecutor::execute(
    const physical_planner::plan::DeletePlan & plan
)
{
    return detail::execute_delete(
        plan,
        context_.catalog,
        context_.storage,
        context_.index_engine,
        context_.vector_index_engine,
        context_.transaction_manager
    );
}

std::expected<ExecutionResult, ExecutionError> MutationExecutor::execute(
    const physical_planner::plan::InsertPlan & plan
)
{
    return detail::execute_insert(
        plan,
        context_.storage,
        context_.transaction_manager
    );
}

std::expected<ExecutionResult, ExecutionError> MutationExecutor::execute(
    const physical_planner::plan::UpdatePlan & plan
)
{
    return detail::execute_update(
        plan,
        context_.catalog,
        context_.storage,
        context_.index_engine,
        context_.vector_index_engine,
        context_.transaction_manager
    );
}

} // namespace litedb::core::executor
