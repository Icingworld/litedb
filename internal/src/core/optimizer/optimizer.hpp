#pragma once

#include <memory>

#include "core/logical_planner/operator/dispatcher/logical_operator_dispatcher.hpp"
#include "core/logical_planner/plan/dispatcher/logical_plan_dispatcher.hpp"

namespace litedb::core::optimizer
{

/**
 * @brief 优化器选项
 */
struct OptimizerOptions
{
    bool enabled {true};                           ///< 是否启用优化器
    bool enable_constant_folding {true};           ///< 是否启用常量折叠
    bool enable_boolean_simplification {true};     ///< 是否启用布尔简化
    bool enable_filter_elimination {true};         ///< 是否启用过滤消除
};

/**
 * @brief 优化器
 */
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
    /**
     * @brief 优化逻辑计划
     * @param plan 逻辑计划
     * @return 优化后的逻辑计划
     */
    [[nodiscard]]
    std::unique_ptr<logical_planner::plan::LogicalPlan> optimize(
        std::unique_ptr<logical_planner::plan::LogicalPlan> plan
    );

private:
    /**
     * @brief 访问 USE 计划
     * @param plan USE 计划
     * @return 优化后的逻辑计划
     */
    [[nodiscard]]
    std::unique_ptr<logical_planner::plan::LogicalPlan>
    visit_use_plan(logical_planner::plan::UsePlan & plan);

    /**
     * @brief 访问 CREATE DATABASE 计划
     * @param plan CREATE DATABASE 计划
     * @return 优化后的逻辑计划
     */
    [[nodiscard]]
    std::unique_ptr<logical_planner::plan::LogicalPlan>
    visit_create_database_plan(logical_planner::plan::CreateDatabasePlan & plan);

    /**
     * @brief 访问 CREATE COLLECTION 计划
     * @param plan CREATE COLLECTION 计划
     * @return 优化后的逻辑计划
     */
    [[nodiscard]]
    std::unique_ptr<logical_planner::plan::LogicalPlan>
    visit_create_collection_plan(
        logical_planner::plan::CreateCollectionPlan & plan
    );

    /**
     * @brief 访问 CREATE INDEX 计划
     * @param plan CREATE INDEX 计划
     * @return 优化后的逻辑计划
     */
    [[nodiscard]]
    std::unique_ptr<logical_planner::plan::LogicalPlan>
    visit_create_index_plan(logical_planner::plan::CreateIndexPlan & plan);

    /**
     * @brief 访问 CREATE VINDEX 计划
     * @param plan CREATE VINDEX 计划
     * @return 优化后的逻辑计划
     */
    [[nodiscard]]
    std::unique_ptr<logical_planner::plan::LogicalPlan>
    visit_create_vector_index_plan(
        logical_planner::plan::CreateVectorIndexPlan & plan
    );

    /**
     * @brief 访问 DROP DATABASE 计划
     * @param plan DROP DATABASE 计划
     * @return 优化后的逻辑计划
     */
    [[nodiscard]]
    std::unique_ptr<logical_planner::plan::LogicalPlan>
    visit_drop_database_plan(logical_planner::plan::DropDatabasePlan & plan);

    /**
     * @brief 访问 DROP COLLECTION 计划
     * @param plan DROP COLLECTION 计划
     * @return 优化后的逻辑计划
     */
    [[nodiscard]]
    std::unique_ptr<logical_planner::plan::LogicalPlan>
    visit_drop_collection_plan(
        logical_planner::plan::DropCollectionPlan & plan
    );

    /**
     * @brief 访问 DROP INDEX 计划
     * @param plan DROP INDEX 计划
     * @return 优化后的逻辑计划
     */
    [[nodiscard]]
    std::unique_ptr<logical_planner::plan::LogicalPlan>
    visit_drop_index_plan(logical_planner::plan::DropIndexPlan & plan);

    /**
     * @brief 访问 DROP VINDEX 计划
     * @param plan DROP VINDEX 计划
     * @return 优化后的逻辑计划
     */
    [[nodiscard]]
    std::unique_ptr<logical_planner::plan::LogicalPlan>
    visit_drop_vector_index_plan(
        logical_planner::plan::DropVectorIndexPlan & plan
    );

    /**
     * @brief 访问 SHOW DATABASES 计划
     * @param plan SHOW DATABASES 计划
     * @return 优化后的逻辑计划
     */
    [[nodiscard]]
    std::unique_ptr<logical_planner::plan::LogicalPlan>
    visit_show_databases_plan(logical_planner::plan::ShowDatabasesPlan & plan);

    /**
     * @brief 访问 SHOW COLLECTIONS 计划
     * @param plan SHOW COLLECTIONS 计划
     * @return 优化后的逻辑计划
     */
    [[nodiscard]]
    std::unique_ptr<logical_planner::plan::LogicalPlan>
    visit_show_collections_plan(
        logical_planner::plan::ShowCollectionsPlan & plan
    );

    /**
     * @brief 访问 SHOW INDEXES 计划
     * @param plan SHOW INDEXES 计划
     * @return 优化后的逻辑计划
     */
    [[nodiscard]]
    std::unique_ptr<logical_planner::plan::LogicalPlan>
    visit_show_indexes_plan(logical_planner::plan::ShowIndexesPlan & plan);

    /**
     * @brief 访问 SHOW VINDEXES 计划
     * @param plan SHOW VINDEXES 计划
     * @return 优化后的逻辑计划
     */
    [[nodiscard]]
    std::unique_ptr<logical_planner::plan::LogicalPlan>
    visit_show_vector_indexes_plan(
        logical_planner::plan::ShowVectorIndexesPlan & plan
    );

    /**
     * @brief 访问 DESCRIBE COLLECTION 计划
     * @param plan DESCRIBE COLLECTION 计划
     * @return 优化后的逻辑计划
     */
    [[nodiscard]]
    std::unique_ptr<logical_planner::plan::LogicalPlan>
    visit_describe_collection_plan(
        logical_planner::plan::DescribeCollectionPlan & plan
    );

    /**
     * @brief 访问 INSERT 计划
     * @param plan INSERT 计划
     * @return 优化后的逻辑计划
     */
    [[nodiscard]]
    std::unique_ptr<logical_planner::plan::LogicalPlan>
    visit_insert_plan(logical_planner::plan::InsertPlan & plan);

    /**
     * @brief 访问 UPDATE 计划
     * @param plan UPDATE 计划
     * @return 优化后的逻辑计划
     */
    [[nodiscard]]
    std::unique_ptr<logical_planner::plan::LogicalPlan>
    visit_update_plan(logical_planner::plan::UpdatePlan & plan);

    /**
     * @brief 访问 DELETE 计划
     * @param plan DELETE 计划
     * @return 优化后的逻辑计划
     */
    [[nodiscard]]
    std::unique_ptr<logical_planner::plan::LogicalPlan>
    visit_delete_plan(logical_planner::plan::DeletePlan & plan);

    /**
     * @brief 访问 QUERY 计划
     * @param plan QUERY 计划
     * @return 优化后的逻辑计划
     */
    [[nodiscard]]
    std::unique_ptr<logical_planner::plan::LogicalPlan>
    visit_query_plan(logical_planner::plan::QueryPlan & plan);

private:
    /**
     * @brief 重写逻辑算子树
     * @param op 逻辑算子
     * @return 重写后的逻辑算子
     */
    [[nodiscard]]
    std::unique_ptr<logical_planner::op::LogicalPlanOperator> rewrite_operator(
        std::unique_ptr<logical_planner::op::LogicalPlanOperator> op
    );

    /**
     * @brief 访问扫描算子
     * @param op 扫描算子
     * @return 重写后的逻辑算子
     */
    [[nodiscard]]
    std::unique_ptr<logical_planner::op::LogicalPlanOperator>
    visit_scan_operator(
        logical_planner::op::LogicalScanOperator & op
    );

    /**
     * @brief 访问过滤算子
     * @param op 过滤算子
     * @return 重写后的逻辑算子
     */
    [[nodiscard]]
    std::unique_ptr<logical_planner::op::LogicalPlanOperator>
    visit_filter_operator(
        logical_planner::op::LogicalFilterOperator & op
    );

    /**
     * @brief 访问投影算子
     * @param op 投影算子
     * @return 重写后的逻辑算子
     */
    [[nodiscard]]
    std::unique_ptr<logical_planner::op::LogicalPlanOperator>
    visit_projection_operator(
        logical_planner::op::LogicalProjectionOperator & op
    );

    /**
     * @brief 访问排序算子
     * @param op 排序算子
     * @return 重写后的逻辑算子
     */
    [[nodiscard]]
    std::unique_ptr<logical_planner::op::LogicalPlanOperator>
    visit_order_by_operator(
        logical_planner::op::LogicalOrderByOperator & op
    );

    /**
     * @brief 访问限制算子
     * @param op 限制算子
     * @return 重写后的逻辑算子
     */
    [[nodiscard]]
    std::unique_ptr<logical_planner::op::LogicalPlanOperator>
    visit_limit_operator(
        logical_planner::op::LogicalLimitOperator & op
    );

private:
    OptimizerOptions options_;                       ///< 优化器选项
};

} // namespace litedb::core::optimizer
