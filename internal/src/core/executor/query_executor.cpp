#include "core/executor/query_executor.hpp"

#include "core/executor/executor_detail.hpp"

namespace litedb::core::executor
{

QueryExecutor::QueryExecutor(ExecutionContext & context) noexcept
    : context_(context)
{
}

std::expected<ExecutionResult, ExecutionError> QueryExecutor::execute(
    const physical_planner::plan::QueryPlan & plan
)
{
    return detail::execute_query(
        plan,
        context_.catalog,
        context_.storage,
        context_.index_engine,
        context_.vector_index_engine
    );
}

} // namespace litedb::core::executor
