#pragma once

#include <memory>

#include "core/logical_planner/operator/dispatcher/logical_operator_dispatcher.hpp"
#include "core/logical_planner/plan/dispatcher/logical_plan_dispatcher.hpp"
#include "core/meta/meta_engine.hpp"
#include "core/physical_planner/operator/dispatcher/physical_operator_dispatcher.hpp"
#include "core/physical_planner/plan/dispatcher/physical_plan_dispatcher.hpp"

namespace litedb::core::physical_planner
{

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

    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan> plan(
        std::unique_ptr<logical_planner::plan::LogicalPlan> logical_plan
    );

private:
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan>
    visit_use_plan(logical_planner::plan::UsePlan & logical_plan);
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan>
    visit_create_database_plan(logical_planner::plan::CreateDatabasePlan & logical_plan);
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan>
    visit_create_collection_plan(logical_planner::plan::CreateCollectionPlan & logical_plan);
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan>
    visit_create_index_plan(logical_planner::plan::CreateIndexPlan & logical_plan);
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan>
    visit_create_vector_index_plan(logical_planner::plan::CreateVectorIndexPlan & logical_plan);
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan>
    visit_drop_database_plan(logical_planner::plan::DropDatabasePlan & logical_plan);
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan>
    visit_drop_collection_plan(logical_planner::plan::DropCollectionPlan & logical_plan);
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan>
    visit_drop_index_plan(logical_planner::plan::DropIndexPlan & logical_plan);
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan>
    visit_drop_vector_index_plan(logical_planner::plan::DropVectorIndexPlan & logical_plan);
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan>
    visit_show_databases_plan(logical_planner::plan::ShowDatabasesPlan & logical_plan);
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan>
    visit_show_collections_plan(logical_planner::plan::ShowCollectionsPlan & logical_plan);
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan>
    visit_show_indexes_plan(logical_planner::plan::ShowIndexesPlan & logical_plan);
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan>
    visit_show_vector_indexes_plan(logical_planner::plan::ShowVectorIndexesPlan & logical_plan);
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan>
    visit_describe_collection_plan(logical_planner::plan::DescribeCollectionPlan & logical_plan);
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan>
    visit_insert_plan(logical_planner::plan::InsertPlan & logical_plan);
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan>
    visit_update_plan(logical_planner::plan::UpdatePlan & logical_plan);
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan>
    visit_delete_plan(logical_planner::plan::DeletePlan & logical_plan);
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan>
    visit_query_plan(logical_planner::plan::QueryPlan & logical_plan);

private:
    [[nodiscard]] std::unique_ptr<op::PhysicalOperator> lower_operator(
        std::unique_ptr<logical_planner::op::LogicalPlanOperator> logical_operator
    );

    [[nodiscard]] std::unique_ptr<op::PhysicalOperator>
    visit_scan_operator(logical_planner::op::LogicalScanOperator & logical_operator);
    [[nodiscard]] std::unique_ptr<op::PhysicalOperator>
    visit_filter_operator(logical_planner::op::LogicalFilterOperator & logical_operator);
    [[nodiscard]] std::unique_ptr<op::PhysicalOperator>
    visit_projection_operator(logical_planner::op::LogicalProjectionOperator & logical_operator);
    [[nodiscard]] std::unique_ptr<op::PhysicalOperator>
    visit_order_by_operator(logical_planner::op::LogicalOrderByOperator & logical_operator);
    [[nodiscard]] std::unique_ptr<op::PhysicalOperator>
    visit_limit_operator(logical_planner::op::LogicalLimitOperator & logical_operator);

private:
    meta::CatalogView catalog_;
};

} // namespace litedb::core::physical_planner
