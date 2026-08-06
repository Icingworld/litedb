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

/**
 * @brief 命令类逻辑计划 lowering worker
 */
class PhysicalCommandWorker final
{
public:
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan> plan_use(
        logical_planner::plan::UsePlan & logical_plan
    );
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan> plan_create_database(
        logical_planner::plan::CreateDatabasePlan & logical_plan
    );
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan> plan_create_collection(
        logical_planner::plan::CreateCollectionPlan & logical_plan
    );
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan> plan_create_index(
        logical_planner::plan::CreateIndexPlan & logical_plan
    );
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan> plan_create_vector_index(
        logical_planner::plan::CreateVectorIndexPlan & logical_plan
    );
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan> plan_drop_database(
        logical_planner::plan::DropDatabasePlan & logical_plan
    );
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan> plan_drop_collection(
        logical_planner::plan::DropCollectionPlan & logical_plan
    );
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan> plan_drop_index(
        logical_planner::plan::DropIndexPlan & logical_plan
    );
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan> plan_drop_vector_index(
        logical_planner::plan::DropVectorIndexPlan & logical_plan
    );
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan> plan_show_databases(
        logical_planner::plan::ShowDatabasesPlan & logical_plan
    );
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan> plan_show_collections(
        logical_planner::plan::ShowCollectionsPlan & logical_plan
    );
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan> plan_show_indexes(
        logical_planner::plan::ShowIndexesPlan & logical_plan
    );
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan> plan_show_vector_indexes(
        logical_planner::plan::ShowVectorIndexesPlan & logical_plan
    );
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan> plan_describe_collection(
        logical_planner::plan::DescribeCollectionPlan & logical_plan
    );
};

} // namespace litedb::core::physical_planner
