#pragma once

#include <memory>

namespace litedb::core::logical_planner::plan
{

class CreateCollectionPlan;
class CreateDatabasePlan;
class CreateIndexPlan;
class CreateVectorIndexPlan;
class DescribeCollectionPlan;
class DropCollectionPlan;
class DropDatabasePlan;
class DropIndexPlan;
class DropVectorIndexPlan;
class ShowCollectionsPlan;
class ShowDatabasesPlan;
class ShowIndexesPlan;
class ShowVectorIndexesPlan;
class UsePlan;

} // namespace litedb::core::logical_planner::plan

namespace litedb::core::physical_planner::plan
{

class PhysicalPlan;

} // namespace litedb::core::physical_planner::plan

namespace litedb::core::physical_planner
{

// 命令类物理计划工作器
class PhysicalCommandWorker final
{
public:
    // 计划 USE 语句
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> plan_use(logical_planner::plan::UsePlan & logical_plan);

    // 计划 CREATE DATABASE 语句
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> plan_create_database(
        logical_planner::plan::CreateDatabasePlan & logical_plan
    );

    // 计划 CREATE COLLECTION 语句
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> plan_create_collection(
        logical_planner::plan::CreateCollectionPlan & logical_plan
    );

    // 计划 CREATE INDEX 语句
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> plan_create_index(
        logical_planner::plan::CreateIndexPlan & logical_plan
    );

    // 计划 CREATE VINDEX 语句
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> plan_create_vector_index(
        logical_planner::plan::CreateVectorIndexPlan & logical_plan
    );

    // 计划 DROP DATABASE 语句
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> plan_drop_database(
        logical_planner::plan::DropDatabasePlan & logical_plan
    );

    // 计划 DROP COLLECTION 语句
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> plan_drop_collection(
        logical_planner::plan::DropCollectionPlan & logical_plan
    );

    // 计划 DROP INDEX 语句
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> plan_drop_index(
        logical_planner::plan::DropIndexPlan & logical_plan
    );

    // 计划 DROP VINDEX 语句
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> plan_drop_vector_index(
        logical_planner::plan::DropVectorIndexPlan & logical_plan
    );

    // 计划 SHOW DATABASES 语句
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> plan_show_databases(
        logical_planner::plan::ShowDatabasesPlan & logical_plan
    );

    // 计划 SHOW COLLECTIONS 语句
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> plan_show_collections(
        logical_planner::plan::ShowCollectionsPlan & logical_plan
    );

    // 计划 SHOW INDEXES 语句
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> plan_show_indexes(
        logical_planner::plan::ShowIndexesPlan & logical_plan
    );

    // 计划 SHOW VINDEXES 语句
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> plan_show_vector_indexes(
        logical_planner::plan::ShowVectorIndexesPlan & logical_plan
    );

    // 计划 DESCRIBE COLLECTION 语句
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> plan_describe_collection(
        logical_planner::plan::DescribeCollectionPlan & logical_plan
    );
};

} // namespace litedb::core::physical_planner
