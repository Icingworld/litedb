#pragma once

#include <expected>

#include "core/executor/execution_context.hpp"
#include "core/executor/execution_error.hpp"
#include "core/executor/execution_result.hpp"
#include "core/physical_planner/plan/query/query_plan.hpp"

namespace litedb::core::executor
{

class QueryExecutor final
{
public:
    explicit QueryExecutor(ExecutionContext & context) noexcept;

    [[nodiscard]]
    std::expected<ExecutionResult, ExecutionError> execute(
        const physical_planner::plan::QueryPlan & plan
    );

private:
    ExecutionContext & context_;
};

} // namespace litedb::core::executor
