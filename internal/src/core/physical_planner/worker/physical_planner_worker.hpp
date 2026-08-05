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

/**
 * @brief 物理计划 statement dispatcher
 */
class PhysicalPlannerWorker final
    : private logical_planner::plan::MutableLogicalPlanDispatcher<
          PhysicalPlannerWorker,
          std::unique_ptr<plan::PhysicalPlan>
      >
{
    friend logical_planner::plan::MutableLogicalPlanDispatcher<
        PhysicalPlannerWorker,
        std::unique_ptr<plan::PhysicalPlan>
    >;

public:
    explicit PhysicalPlannerWorker(const PhysicalPlannerContext & context) noexcept
        : context_(context)
    {
    }

public:
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> plan_statement(
        std::unique_ptr<logical_planner::plan::LogicalPlan> logical_plan
    );

private:
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan> visit_use_plan(
        logical_planner::plan::UsePlan & logical_plan
    );
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan> visit_create_database_plan(
        logical_planner::plan::CreateDatabasePlan & logical_plan
    );
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan> visit_create_collection_plan(
        logical_planner::plan::CreateCollectionPlan & logical_plan
    );
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan> visit_create_index_plan(
        logical_planner::plan::CreateIndexPlan & logical_plan
    );
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan> visit_create_vector_index_plan(
        logical_planner::plan::CreateVectorIndexPlan & logical_plan
    );
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan> visit_drop_database_plan(
        logical_planner::plan::DropDatabasePlan & logical_plan
    );
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan> visit_drop_collection_plan(
        logical_planner::plan::DropCollectionPlan & logical_plan
    );
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan> visit_drop_index_plan(
        logical_planner::plan::DropIndexPlan & logical_plan
    );
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan> visit_drop_vector_index_plan(
        logical_planner::plan::DropVectorIndexPlan & logical_plan
    );
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan> visit_show_databases_plan(
        logical_planner::plan::ShowDatabasesPlan & logical_plan
    );
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan> visit_show_collections_plan(
        logical_planner::plan::ShowCollectionsPlan & logical_plan
    );
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan> visit_show_indexes_plan(
        logical_planner::plan::ShowIndexesPlan & logical_plan
    );
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan> visit_show_vector_indexes_plan(
        logical_planner::plan::ShowVectorIndexesPlan & logical_plan
    );
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan> visit_describe_collection_plan(
        logical_planner::plan::DescribeCollectionPlan & logical_plan
    );
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan> visit_insert_plan(
        logical_planner::plan::InsertPlan & logical_plan
    );
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan> visit_update_plan(
        logical_planner::plan::UpdatePlan & logical_plan
    );
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan> visit_delete_plan(
        logical_planner::plan::DeletePlan & logical_plan
    );
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan> visit_query_plan(
        logical_planner::plan::QueryPlan & logical_plan
    );

private:
    const PhysicalPlannerContext & context_;
};

} // namespace litedb::core::physical_planner
