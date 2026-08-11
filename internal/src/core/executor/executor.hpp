#pragma once

#include <expected>

#include "core/executor/execution_context.hpp"
#include "core/executor/execution_error.hpp"
#include "core/executor/execution_result.hpp"
#include "core/physical_planner/plan/dispatcher/physical_plan_dispatcher.hpp"

namespace litedb::core::executor
{

// 物理计划执行器
class Executor final
    : private physical_planner::plan::
          ConstPhysicalPlanDispatcher<Executor, std::expected<ExecutionResult, ExecutionError>>
{
    using Result = std::expected<ExecutionResult, ExecutionError>;
    using Dispatcher = physical_planner::plan::ConstPhysicalPlanDispatcher<Executor, Result>;

    friend Dispatcher;

public:
    explicit Executor(ExecutionContext context) noexcept;

public:
    // 执行物理计划
    [[nodiscard]]
    Result execute(const physical_planner::plan::PhysicalPlan & plan);

private:
    // 访问 USE 计划
    [[nodiscard]]
    Result visit_use_plan(const physical_planner::plan::UsePlan & plan);

    // 访问 CREATE DATABASE 计划
    [[nodiscard]]
    Result visit_create_database_plan(const physical_planner::plan::CreateDatabasePlan & plan);

    // 访问 CREATE COLLECTION 计划
    [[nodiscard]]
    Result visit_create_collection_plan(const physical_planner::plan::CreateCollectionPlan & plan);

    // 访问 CREATE INDEX 计划
    [[nodiscard]]
    Result visit_create_index_plan(const physical_planner::plan::CreateIndexPlan & plan);

    // 访问 CREATE VINDEX 计划
    [[nodiscard]]
    Result visit_create_vector_index_plan(
        const physical_planner::plan::CreateVectorIndexPlan & plan
    );

    // 访问 DROP DATABASE 计划
    [[nodiscard]]
    Result visit_drop_database_plan(const physical_planner::plan::DropDatabasePlan & plan);

    // 访问 DROP COLLECTION 计划
    [[nodiscard]]
    Result visit_drop_collection_plan(const physical_planner::plan::DropCollectionPlan & plan);

    // 访问 DROP INDEX 计划
    [[nodiscard]]
    Result visit_drop_index_plan(const physical_planner::plan::DropIndexPlan & plan);

    // 访问 DROP VINDEX 计划
    [[nodiscard]]
    Result visit_drop_vector_index_plan(const physical_planner::plan::DropVectorIndexPlan & plan);

    // 访问 SHOW DATABASES 计划
    [[nodiscard]]
    Result visit_show_databases_plan(const physical_planner::plan::ShowDatabasesPlan & plan);

    // 访问 SHOW COLLECTIONS 计划
    [[nodiscard]]
    Result visit_show_collections_plan(const physical_planner::plan::ShowCollectionsPlan & plan);

    // 访问 SHOW INDEXES 计划
    [[nodiscard]]
    Result visit_show_indexes_plan(const physical_planner::plan::ShowIndexesPlan & plan);

    // 访问 SHOW VINDEXES 计划
    [[nodiscard]]
    Result visit_show_vector_indexes_plan(
        const physical_planner::plan::ShowVectorIndexesPlan & plan
    );

    // 访问 DESCRIBE COLLECTION 计划
    [[nodiscard]]
    Result visit_describe_collection_plan(
        const physical_planner::plan::DescribeCollectionPlan & plan
    );

    // 访问 INSERT 计划
    [[nodiscard]]
    Result visit_insert_plan(const physical_planner::plan::InsertPlan & plan);

    // 访问 UPDATE 计划
    [[nodiscard]]
    Result visit_update_plan(const physical_planner::plan::UpdatePlan & plan);

    // 访问 DELETE 计划
    [[nodiscard]]
    Result visit_delete_plan(const physical_planner::plan::DeletePlan & plan);

    // 访问 QUERY 计划
    [[nodiscard]]
    Result visit_query_plan(const physical_planner::plan::QueryPlan & plan);

private:
    ExecutionContext context_;
};

} // namespace litedb::core::executor
