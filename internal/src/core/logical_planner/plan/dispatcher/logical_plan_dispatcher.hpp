#pragma once

#include <utility>

#include "core/logical_planner/plan/command/create_collection_plan.hpp"
#include "core/logical_planner/plan/command/create_database_plan.hpp"
#include "core/logical_planner/plan/command/create_index_plan.hpp"
#include "core/logical_planner/plan/command/create_vector_index_plan.hpp"
#include "core/logical_planner/plan/command/describe_collection_plan.hpp"
#include "core/logical_planner/plan/command/drop_collection_plan.hpp"
#include "core/logical_planner/plan/command/drop_database_plan.hpp"
#include "core/logical_planner/plan/command/drop_index_plan.hpp"
#include "core/logical_planner/plan/command/drop_vector_index_plan.hpp"
#include "core/logical_planner/plan/command/show_collections_plan.hpp"
#include "core/logical_planner/plan/command/show_databases_plan.hpp"
#include "core/logical_planner/plan/command/show_indexes_plan.hpp"
#include "core/logical_planner/plan/command/show_vector_indexes_plan.hpp"
#include "core/logical_planner/plan/command/use_plan.hpp"
#include "core/logical_planner/plan/logical_plan.hpp"
#include "core/logical_planner/plan/mutation/delete_plan.hpp"
#include "core/logical_planner/plan/mutation/insert_plan.hpp"
#include "core/logical_planner/plan/mutation/update_plan.hpp"
#include "core/logical_planner/plan/query/query_plan.hpp"

namespace litedb::core::logical_planner::plan
{

/**
 * @brief 逻辑计划调度器
 * @tparam Derived 派生类
 * @tparam ReturnType 返回类型，默认为 void
 */
template <typename Derived, typename ReturnType = void>
class LogicalPlanDispatcher
{
protected:
    /**
     * @brief 调度逻辑计划
     * @param plan 逻辑计划
     * @return 返回值
     */
    [[nodiscard]]
    ReturnType dispatch_plan(const LogicalPlan & plan)
    {
        switch (plan.kind()) {
        // command
        case LogicalPlanKind::Use:
            return derived().visit_use_plan(
                static_cast<const UsePlan &>(plan)
            );
        case LogicalPlanKind::CreateDatabase:
            return derived().visit_create_database_plan(
                static_cast<const CreateDatabasePlan &>(plan)
            );
        case LogicalPlanKind::CreateCollection:
            return derived().visit_create_collection_plan(
                static_cast<const CreateCollectionPlan &>(plan)
            );
        case LogicalPlanKind::CreateIndex:
            return derived().visit_create_index_plan(
                static_cast<const CreateIndexPlan &>(plan)
            );
        case LogicalPlanKind::CreateVectorIndex:
            return derived().visit_create_vector_index_plan(
                static_cast<const CreateVectorIndexPlan &>(plan)
            );
        case LogicalPlanKind::DropDatabase:
            return derived().visit_drop_database_plan(
                static_cast<const DropDatabasePlan &>(plan)
            );
        case LogicalPlanKind::DropCollection:
            return derived().visit_drop_collection_plan(
                static_cast<const DropCollectionPlan &>(plan)
            );
        case LogicalPlanKind::DropIndex:
            return derived().visit_drop_index_plan(
                static_cast<const DropIndexPlan &>(plan)
            );
        case LogicalPlanKind::DropVectorIndex:
            return derived().visit_drop_vector_index_plan(
                static_cast<const DropVectorIndexPlan &>(plan)
            );
        case LogicalPlanKind::ShowDatabases:
            return derived().visit_show_databases_plan(
                static_cast<const ShowDatabasesPlan &>(plan)
            );
        case LogicalPlanKind::ShowCollections:
            return derived().visit_show_collections_plan(
                static_cast<const ShowCollectionsPlan &>(plan)
            );
        case LogicalPlanKind::ShowIndexes:
            return derived().visit_show_indexes_plan(
                static_cast<const ShowIndexesPlan &>(plan)
            );
        case LogicalPlanKind::ShowVectorIndexes:
            return derived().visit_show_vector_indexes_plan(
                static_cast<const ShowVectorIndexesPlan &>(plan)
            );
        case LogicalPlanKind::DescribeCollection:
            return derived().visit_describe_collection_plan(
                static_cast<const DescribeCollectionPlan &>(plan)
            );

        // mutation
        case LogicalPlanKind::Insert:
            return derived().visit_insert_plan(
                static_cast<const InsertPlan &>(plan)
            );
        case LogicalPlanKind::Update:
            return derived().visit_update_plan(
                static_cast<const UpdatePlan &>(plan)
            );
        case LogicalPlanKind::Delete:
            return derived().visit_delete_plan(
                static_cast<const DeletePlan &>(plan)
            );

        // query
        case LogicalPlanKind::Query:
            return derived().visit_query_plan(
                static_cast<const QueryPlan &>(plan)
            );
        default:
            std::unreachable();
        }
    }

private:
    /**
     * @brief 获取派生类引用
     * @return 派生类引用
     */
    [[nodiscard]]
    Derived & derived() noexcept
    {
        return static_cast<Derived &>(*this);
    }
};

} // namespace litedb::core::logical_planner::plan
