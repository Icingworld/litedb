#pragma once

#include <memory>

#include "core/logical_planner/operator/dispatcher/logical_operator_dispatcher.hpp"
#include "core/logical_planner/plan/dispatcher/logical_plan_dispatcher.hpp"
#include "core/meta/meta_engine.hpp"
#include "core/physical_planner/operator/dispatcher/physical_operator_dispatcher.hpp"
#include "core/physical_planner/plan/dispatcher/physical_plan_dispatcher.hpp"

namespace litedb::core::physical_planner
{

/**
 * @brief 物理计划器
 * @details 将逻辑计划降级为物理计划
 */
class PhysicalPlanner final
    : private logical_planner::plan::MutableLogicalPlanDispatcher<
          PhysicalPlanner,
          std::unique_ptr<plan::PhysicalPlan>
      >
    , private logical_planner::op::MutableLogicalOperatorDispatcher<
          PhysicalPlanner,
          std::unique_ptr<op::PhysicalOperator>
      >
{
    friend logical_planner::plan::MutableLogicalPlanDispatcher<
        PhysicalPlanner,
        std::unique_ptr<plan::PhysicalPlan>
    >;
    friend logical_planner::op::MutableLogicalOperatorDispatcher<
        PhysicalPlanner,
        std::unique_ptr<op::PhysicalOperator>
    >;

public:
    explicit PhysicalPlanner(meta::CatalogView catalog) noexcept;

public:
    /**
     * @brief 生成物理计划
     * @param logical_plan 逻辑计划
     * @return 物理计划
     * @pre logical_plan != nullptr
     * @warning 该成员函数会消费 logical_plan 的所有权
     */
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> plan(
        std::unique_ptr<logical_planner::plan::LogicalPlan> logical_plan
    );

private:
    /**
     * @brief 访问 USE 计划
     * @param logical_plan USE 计划
     * @return 物理计划
     */
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> visit_use_plan(
        logical_planner::plan::UsePlan & logical_plan
    );

    /**
     * @brief 访问 CREATE DATABASE 计划
     * @param logical_plan CREATE DATABASE 计划
     * @return 物理计划
     */
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> visit_create_database_plan(
        logical_planner::plan::CreateDatabasePlan & logical_plan
    );

    /**
     * @brief 访问 CREATE COLLECTION 计划
     * @param logical_plan CREATE COLLECTION 计划
     * @return 物理计划
     */
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> visit_create_collection_plan(
        logical_planner::plan::CreateCollectionPlan & logical_plan
    );

    /**
     * @brief 访问 CREATE INDEX 计划
     * @param logical_plan CREATE INDEX 计划
     * @return 物理计划
     */
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> visit_create_index_plan(
        logical_planner::plan::CreateIndexPlan & logical_plan
    );

    /**
     * @brief 访问 CREATE VINDEX 计划
     * @param logical_plan CREATE VINDEX 计划
     * @return 物理计划
     */
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> visit_create_vector_index_plan(
        logical_planner::plan::CreateVectorIndexPlan & logical_plan
    );

    /**
     * @brief 访问 DROP DATABASE 计划
     * @param logical_plan DROP DATABASE 计划
     * @return 物理计划
     */
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> visit_drop_database_plan(
        logical_planner::plan::DropDatabasePlan & logical_plan
    );

    /**
     * @brief 访问 DROP COLLECTION 计划
     * @param logical_plan DROP COLLECTION 计划
     * @return 物理计划
     */
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> visit_drop_collection_plan(
        logical_planner::plan::DropCollectionPlan & logical_plan
    );

    /**
     * @brief 访问 DROP INDEX 计划
     * @param logical_plan DROP INDEX 计划
     * @return 物理计划
     */
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> visit_drop_index_plan(
        logical_planner::plan::DropIndexPlan & logical_plan
    );

    /**
     * @brief 访问 DROP VINDEX 计划
     * @param logical_plan DROP VINDEX 计划
     * @return 物理计划
     */
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> visit_drop_vector_index_plan(
        logical_planner::plan::DropVectorIndexPlan & logical_plan
    );

    /**
     * @brief 访问 SHOW DATABASES 计划
     * @param logical_plan SHOW DATABASES 计划
     * @return 物理计划
     */
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> visit_show_databases_plan(
        logical_planner::plan::ShowDatabasesPlan & logical_plan
    );

    /**
     * @brief 访问 SHOW COLLECTIONS 计划
     * @param logical_plan SHOW COLLECTIONS 计划
     * @return 物理计划
     */
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> visit_show_collections_plan(
        logical_planner::plan::ShowCollectionsPlan & logical_plan
    );

    /**
     * @brief 访问 SHOW INDEXES 计划
     * @param logical_plan SHOW INDEXES 计划
     * @return 物理计划
     */
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> visit_show_indexes_plan(
        logical_planner::plan::ShowIndexesPlan & logical_plan
    );

    /**
     * @brief 访问 SHOW VINDEXES 计划
     * @param logical_plan SHOW VINDEXES 计划
     * @return 物理计划
     */
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> visit_show_vector_indexes_plan(
        logical_planner::plan::ShowVectorIndexesPlan & logical_plan
    );

    /**
     * @brief 访问 DESCRIBE COLLECTION 计划
     * @param logical_plan DESCRIBE COLLECTION 计划
     * @return 物理计划
     */
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> visit_describe_collection_plan(
        logical_planner::plan::DescribeCollectionPlan & logical_plan
    );

    /**
     * @brief 访问 INSERT 计划
     * @param logical_plan INSERT 计划
     * @return 物理计划
     */
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> visit_insert_plan(
        logical_planner::plan::InsertPlan & logical_plan
    );

    /**
     * @brief 访问 UPDATE 计划
     * @param logical_plan UPDATE 计划
     * @return 物理计划
     */
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> visit_update_plan(
        logical_planner::plan::UpdatePlan & logical_plan
    );

    /**
     * @brief 访问 DELETE 计划
     * @param logical_plan DELETE 计划
     * @return 物理计划
     */
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> visit_delete_plan(
        logical_planner::plan::DeletePlan & logical_plan
    );

    /**
     * @brief 访问 QUERY 计划
     * @param logical_plan QUERY 计划
     * @return 物理计划
     */
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> visit_query_plan(
        logical_planner::plan::QueryPlan & logical_plan
    );

private:
    /**
     * @brief 降级逻辑算子
     * @param logical_operator 逻辑算子
     * @return 物理算子
     * @pre logical_operator != nullptr
     * @warning 该成员函数会消费 logical_operator 的所有权
     */
    [[nodiscard]]
    std::unique_ptr<op::PhysicalOperator> lower_operator(
        std::unique_ptr<logical_planner::op::LogicalPlanOperator> logical_operator
    );

    /**
     * @brief 访问扫描算子
     * @param logical_operator 扫描算子
     * @return 物理算子
     */
    [[nodiscard]]
    std::unique_ptr<op::PhysicalOperator> visit_scan_operator(
        logical_planner::op::LogicalScanOperator & logical_operator
    );

    /**
     * @brief 访问过滤算子
     * @param logical_operator 过滤算子
     * @return 物理算子
     */
    [[nodiscard]]
    std::unique_ptr<op::PhysicalOperator> visit_filter_operator(
        logical_planner::op::LogicalFilterOperator & logical_operator
    );

    /**
     * @brief 访问投影算子
     * @param logical_operator 投影算子
     * @return 物理算子
     */
    [[nodiscard]]
    std::unique_ptr<op::PhysicalOperator> visit_projection_operator(
        logical_planner::op::LogicalProjectionOperator & logical_operator
    );

    /**
     * @brief 访问排序算子
     * @param logical_operator 排序算子
     * @return 物理算子
     */
    [[nodiscard]]
    std::unique_ptr<op::PhysicalOperator> visit_order_by_operator(
        logical_planner::op::LogicalOrderByOperator & logical_operator
    );

    /**
     * @brief 访问限制算子
     * @param logical_operator 限制算子
     * @return 物理算子
     */
    [[nodiscard]]
    std::unique_ptr<op::PhysicalOperator> visit_limit_operator(
        logical_planner::op::LogicalLimitOperator & logical_operator
    );

private:
    meta::CatalogView catalog_;             ///< 目录视图
};

} // namespace litedb::core::physical_planner
