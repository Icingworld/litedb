#pragma once

#include <expected>

#include "core/executor/execution_context.hpp"
#include "core/executor/execution_error.hpp"
#include "core/executor/execution_result.hpp"
#include "core/physical_planner/plan/mutation/delete_plan.hpp"
#include "core/physical_planner/plan/mutation/insert_plan.hpp"
#include "core/physical_planner/plan/mutation/update_plan.hpp"

namespace litedb::core::executor
{

class MutationExecutor final
{
public:
    explicit MutationExecutor(ExecutionContext & context) noexcept;

    [[nodiscard]]
    std::expected<ExecutionResult, ExecutionError> execute(
        const physical_planner::plan::DeletePlan & plan
    );

    [[nodiscard]]
    std::expected<ExecutionResult, ExecutionError> execute(
        const physical_planner::plan::InsertPlan & plan
    );

    [[nodiscard]]
    std::expected<ExecutionResult, ExecutionError> execute(
        const physical_planner::plan::UpdatePlan & plan
    );

private:
    ExecutionContext & context_;
};

} // namespace litedb::core::executor
