#pragma once

#include <expected>

#include "core/executor/execution_context.hpp"
#include "core/executor/execution_error.hpp"
#include "core/executor/execution_result.hpp"

namespace litedb::core::physical_planner::plan
{

class DescribeCollectionPlan;
class DeletePlan;
class InsertPlan;
class QueryPlan;
class ShowCollectionsPlan;
class ShowDatabasesPlan;
class ShowIndexesPlan;
class ShowVectorIndexesPlan;
class UpdatePlan;
class UsePlan;

} // namespace litedb::core::physical_planner::plan

namespace litedb::core::executor
{

class Executor
{
public:
    explicit Executor(ExecutionContext context) noexcept;

    [[nodiscard]]
    std::expected<ExecutionResult, ExecutionError> execute(
        const physical_planner::plan::DescribeCollectionPlan & plan
    );

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
        const physical_planner::plan::QueryPlan & plan
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
        const physical_planner::plan::UpdatePlan & plan
    );

    [[nodiscard]]
    std::expected<ExecutionResult, ExecutionError> execute(
        const physical_planner::plan::UsePlan & plan
    );

private:
    ExecutionContext context_;
};

} // namespace litedb::core::executor
