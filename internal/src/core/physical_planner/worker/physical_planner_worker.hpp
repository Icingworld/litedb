#pragma once

#include <memory>

#include "core/logical_planner/plan/dispatcher/logical_plan_dispatcher.hpp"
#include "core/physical_planner/physical_planner_context.hpp"

namespace litedb::core::physical_planner::plan
{

class PhysicalPlan;

} // namespace litedb::core::physical_planner::plan

namespace litedb::core::physical_planner
{

// 物理计划主工作器
class PhysicalPlannerWorker final
    : private logical_planner::plan::
          MutableLogicalPlanDispatcher<PhysicalPlannerWorker, std::unique_ptr<plan::PhysicalPlan>>
{
    friend logical_planner::plan::
        MutableLogicalPlanDispatcher<PhysicalPlannerWorker, std::unique_ptr<plan::PhysicalPlan>>;

public:
    explicit PhysicalPlannerWorker(const PhysicalPlannerContext & context) noexcept;

public:
    // 计划语句
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> plan_statement(
        std::unique_ptr<logical_planner::plan::LogicalPlan> logical_plan
    );

private:
    // 访问 USE 计划
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> visit_use_plan(
        logical_planner::plan::UsePlan & logical_plan
    );

    // 访问 CREATE DATABASE 计划
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> visit_create_database_plan(
        logical_planner::plan::CreateDatabasePlan & logical_plan
    );

    // 访问 CREATE COLLECTION 计划
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> visit_create_collection_plan(
        logical_planner::plan::CreateCollectionPlan & logical_plan
    );

    // 访问 CREATE INDEX 计划
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> visit_create_index_plan(
        logical_planner::plan::CreateIndexPlan & logical_plan
    );

    // 访问 CREATE VINDEX 计划
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> visit_create_vector_index_plan(
        logical_planner::plan::CreateVectorIndexPlan & logical_plan
    );

    // 访问 DROP DATABASE 计划
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> visit_drop_database_plan(
        logical_planner::plan::DropDatabasePlan & logical_plan
    );

    // 访问 DROP COLLECTION 计划
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> visit_drop_collection_plan(
        logical_planner::plan::DropCollectionPlan & logical_plan
    );

    // 访问 DROP INDEX 计划
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> visit_drop_index_plan(
        logical_planner::plan::DropIndexPlan & logical_plan
    );

    // 访问 DROP VINDEX 计划
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> visit_drop_vector_index_plan(
        logical_planner::plan::DropVectorIndexPlan & logical_plan
    );

    // 访问 SHOW DATABASES 计划
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> visit_show_databases_plan(
        logical_planner::plan::ShowDatabasesPlan & logical_plan
    );

    // 访问 SHOW COLLECTIONS 计划
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> visit_show_collections_plan(
        logical_planner::plan::ShowCollectionsPlan & logical_plan
    );

    // 访问 SHOW INDEXES 计划
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> visit_show_indexes_plan(
        logical_planner::plan::ShowIndexesPlan & logical_plan
    );

    // 访问 SHOW VINDEXES 计划
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> visit_show_vector_indexes_plan(
        logical_planner::plan::ShowVectorIndexesPlan & logical_plan
    );

    // 访问 DESCRIBE COLLECTION 计划
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> visit_describe_collection_plan(
        logical_planner::plan::DescribeCollectionPlan & logical_plan
    );

    // 访问 INSERT 计划
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> visit_insert_plan(
        logical_planner::plan::InsertPlan & logical_plan
    );

    // 访问 UPDATE 计划
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> visit_update_plan(
        logical_planner::plan::UpdatePlan & logical_plan
    );

    // 访问 DELETE 计划
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> visit_delete_plan(
        logical_planner::plan::DeletePlan & logical_plan
    );

    // 访问 QUERY 计划
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> visit_query_plan(
        logical_planner::plan::QueryPlan & logical_plan
    );

private:
    const PhysicalPlannerContext & context_;
};

} // namespace litedb::core::physical_planner
