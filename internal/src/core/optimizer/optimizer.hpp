#pragma once

#include <memory>

#include "core/logical_planner/operator/dispatcher/logical_operator_dispatcher.hpp"
#include "core/logical_planner/plan/dispatcher/logical_plan_dispatcher.hpp"

namespace litedb::core::optimizer
{

// 优化器选项
struct OptimizerOptions
{
    bool enabled {true};                           // 是否启用优化器
    bool enable_constant_folding {true};           // 是否启用常量折叠
    bool enable_boolean_simplification {true};     // 是否启用布尔简化
    bool enable_filter_elimination {true};         // 是否启用过滤消除
};

// 优化器
class Optimizer final
    : private logical_planner::plan::MutableLogicalPlanDispatcher<
          Optimizer,
          std::unique_ptr<logical_planner::plan::LogicalPlan>
      >
    , private logical_planner::op::MutableLogicalOperatorDispatcher<
          Optimizer,
          std::unique_ptr<logical_planner::op::LogicalPlanOperator>
      >
{
    friend logical_planner::plan::MutableLogicalPlanDispatcher<
        Optimizer,
        std::unique_ptr<logical_planner::plan::LogicalPlan>
    >;
    friend logical_planner::op::MutableLogicalOperatorDispatcher<
        Optimizer,
        std::unique_ptr<logical_planner::op::LogicalPlanOperator>
    >;

public:
    explicit Optimizer(OptimizerOptions options = {}) noexcept;

public:
    // 优化逻辑计划
    [[nodiscard]]
    std::unique_ptr<logical_planner::plan::LogicalPlan> optimize(
        std::unique_ptr<logical_planner::plan::LogicalPlan> plan
    );

private:
    // 访问 USE 计划
    [[nodiscard]]
    std::unique_ptr<logical_planner::plan::LogicalPlan>
    visit_use_plan(logical_planner::plan::UsePlan & plan);

    // 访问 CREATE DATABASE 计划
    [[nodiscard]]
    std::unique_ptr<logical_planner::plan::LogicalPlan>
    visit_create_database_plan(logical_planner::plan::CreateDatabasePlan & plan);

    // 访问 CREATE COLLECTION 计划
    [[nodiscard]]
    std::unique_ptr<logical_planner::plan::LogicalPlan>
    visit_create_collection_plan(
        logical_planner::plan::CreateCollectionPlan & plan
    );

    // 访问 CREATE INDEX 计划
    [[nodiscard]]
    std::unique_ptr<logical_planner::plan::LogicalPlan>
    visit_create_index_plan(logical_planner::plan::CreateIndexPlan & plan);

    // 访问 CREATE VINDEX 计划
    [[nodiscard]]
    std::unique_ptr<logical_planner::plan::LogicalPlan>
    visit_create_vector_index_plan(
        logical_planner::plan::CreateVectorIndexPlan & plan
    );

    // 访问 DROP DATABASE 计划
    [[nodiscard]]
    std::unique_ptr<logical_planner::plan::LogicalPlan>
    visit_drop_database_plan(logical_planner::plan::DropDatabasePlan & plan);

    // 访问 DROP COLLECTION 计划
    [[nodiscard]]
    std::unique_ptr<logical_planner::plan::LogicalPlan>
    visit_drop_collection_plan(
        logical_planner::plan::DropCollectionPlan & plan
    );

    // 访问 DROP INDEX 计划
    [[nodiscard]]
    std::unique_ptr<logical_planner::plan::LogicalPlan>
    visit_drop_index_plan(logical_planner::plan::DropIndexPlan & plan);

    // 访问 DROP VINDEX 计划
    [[nodiscard]]
    std::unique_ptr<logical_planner::plan::LogicalPlan>
    visit_drop_vector_index_plan(
        logical_planner::plan::DropVectorIndexPlan & plan
    );

    // 访问 SHOW DATABASES 计划
    [[nodiscard]]
    std::unique_ptr<logical_planner::plan::LogicalPlan>
    visit_show_databases_plan(logical_planner::plan::ShowDatabasesPlan & plan);

    // 访问 SHOW COLLECTIONS 计划
    [[nodiscard]]
    std::unique_ptr<logical_planner::plan::LogicalPlan>
    visit_show_collections_plan(
        logical_planner::plan::ShowCollectionsPlan & plan
    );

    // 访问 SHOW INDEXES 计划
    [[nodiscard]]
    std::unique_ptr<logical_planner::plan::LogicalPlan>
    visit_show_indexes_plan(logical_planner::plan::ShowIndexesPlan & plan);

    // 访问 SHOW VINDEXES 计划
    [[nodiscard]]
    std::unique_ptr<logical_planner::plan::LogicalPlan>
    visit_show_vector_indexes_plan(
        logical_planner::plan::ShowVectorIndexesPlan & plan
    );

    // 访问 DESCRIBE COLLECTION 计划
    [[nodiscard]]
    std::unique_ptr<logical_planner::plan::LogicalPlan>
    visit_describe_collection_plan(
        logical_planner::plan::DescribeCollectionPlan & plan
    );

    // 访问 INSERT 计划
    [[nodiscard]]
    std::unique_ptr<logical_planner::plan::LogicalPlan>
    visit_insert_plan(logical_planner::plan::InsertPlan & plan);

    // 访问 UPDATE 计划
    [[nodiscard]]
    std::unique_ptr<logical_planner::plan::LogicalPlan>
    visit_update_plan(logical_planner::plan::UpdatePlan & plan);

    // 访问 DELETE 计划
    [[nodiscard]]
    std::unique_ptr<logical_planner::plan::LogicalPlan>
    visit_delete_plan(logical_planner::plan::DeletePlan & plan);

    // 访问 QUERY 计划
    [[nodiscard]]
    std::unique_ptr<logical_planner::plan::LogicalPlan>
    visit_query_plan(logical_planner::plan::QueryPlan & plan);

private:
    // 重写逻辑算子树
    [[nodiscard]]
    std::unique_ptr<logical_planner::op::LogicalPlanOperator> rewrite_operator(
        std::unique_ptr<logical_planner::op::LogicalPlanOperator> op
    );

    // 访问扫描算子
    [[nodiscard]]
    std::unique_ptr<logical_planner::op::LogicalPlanOperator>
    visit_scan_operator(
        logical_planner::op::LogicalScanOperator & op
    );

    // 访问过滤算子
    [[nodiscard]]
    std::unique_ptr<logical_planner::op::LogicalPlanOperator>
    visit_filter_operator(
        logical_planner::op::LogicalFilterOperator & op
    );

    // 访问投影算子
    [[nodiscard]]
    std::unique_ptr<logical_planner::op::LogicalPlanOperator>
    visit_projection_operator(
        logical_planner::op::LogicalProjectionOperator & op
    );

    // 访问排序算子
    [[nodiscard]]
    std::unique_ptr<logical_planner::op::LogicalPlanOperator>
    visit_order_by_operator(
        logical_planner::op::LogicalOrderByOperator & op
    );

    // 访问限制算子
    [[nodiscard]]
    std::unique_ptr<logical_planner::op::LogicalPlanOperator>
    visit_limit_operator(
        logical_planner::op::LogicalLimitOperator & op
    );

private:
    OptimizerOptions options_;
};

} // namespace litedb::core::optimizer
