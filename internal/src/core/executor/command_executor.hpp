#pragma once

#include <expected>

#include "core/executor/execution_context.hpp"
#include "core/executor/execution_error.hpp"
#include "core/executor/execution_result.hpp"
#include "core/physical_planner/plan/command/describe_collection_plan.hpp"
#include "core/physical_planner/plan/command/show_collections_plan.hpp"
#include "core/physical_planner/plan/command/show_databases_plan.hpp"
#include "core/physical_planner/plan/command/show_indexes_plan.hpp"
#include "core/physical_planner/plan/command/show_vector_indexes_plan.hpp"
#include "core/physical_planner/plan/command/use_plan.hpp"

namespace litedb::core::executor
{

class CommandExecutor final
{
public:
    explicit CommandExecutor(ExecutionContext & context) noexcept;

    [[nodiscard]]
    std::expected<ExecutionResult, ExecutionError> execute(
        const physical_planner::plan::DescribeCollectionPlan & plan
    );

    [[nodiscard]]
    std::expected<ExecutionResult, ExecutionError> execute(
        const physical_planner::plan::ShowCollectionsPlan & plan
    );

    [[nodiscard]]
    std::expected<ExecutionResult, ExecutionError> execute(
        const physical_planner::plan::ShowDatabasesPlan & plan
    );

    [[nodiscard]]
    std::expected<ExecutionResult, ExecutionError> execute(
        const physical_planner::plan::ShowIndexesPlan & plan
    );

    [[nodiscard]]
    std::expected<ExecutionResult, ExecutionError> execute(
        const physical_planner::plan::ShowVectorIndexesPlan & plan
    );

    [[nodiscard]]
    std::expected<ExecutionResult, ExecutionError> execute(
        const physical_planner::plan::UsePlan & plan
    );

private:
    ExecutionContext & context_;
};

} // namespace litedb::core::executor
