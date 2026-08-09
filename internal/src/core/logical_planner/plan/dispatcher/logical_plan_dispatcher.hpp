#pragma once

#include <type_traits>
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

// 逻辑计划调度器
template <typename Derived, typename ReturnType, bool IsConst>
class LogicalPlanDispatcher
{
protected:
    // 引用类型
    template <typename T>
    using ReferenceType = std::conditional_t<IsConst, const T &, T &>;

protected:
    // 调度逻辑计划
    [[nodiscard]]
    ReturnType dispatch_plan(ReferenceType<LogicalPlan> plan)
    {
        switch (plan.kind()) {
        // command
        case LogicalPlanKind::Use:
            return derived().visit_use_plan(static_cast<ReferenceType<UsePlan>>(plan));
        case LogicalPlanKind::CreateDatabase:
            return derived().visit_create_database_plan(
                static_cast<ReferenceType<CreateDatabasePlan>>(plan)
            );
        case LogicalPlanKind::CreateCollection:
            return derived().visit_create_collection_plan(
                static_cast<ReferenceType<CreateCollectionPlan>>(plan)
            );
        case LogicalPlanKind::CreateIndex:
            return derived().visit_create_index_plan(
                static_cast<ReferenceType<CreateIndexPlan>>(plan)
            );
        case LogicalPlanKind::CreateVectorIndex:
            return derived().visit_create_vector_index_plan(
                static_cast<ReferenceType<CreateVectorIndexPlan>>(plan)
            );
        case LogicalPlanKind::DropDatabase:
            return derived().visit_drop_database_plan(
                static_cast<ReferenceType<DropDatabasePlan>>(plan)
            );
        case LogicalPlanKind::DropCollection:
            return derived().visit_drop_collection_plan(
                static_cast<ReferenceType<DropCollectionPlan>>(plan)
            );
        case LogicalPlanKind::DropIndex:
            return derived().visit_drop_index_plan(static_cast<ReferenceType<DropIndexPlan>>(plan));
        case LogicalPlanKind::DropVectorIndex:
            return derived().visit_drop_vector_index_plan(
                static_cast<ReferenceType<DropVectorIndexPlan>>(plan)
            );
        case LogicalPlanKind::ShowDatabases:
            return derived().visit_show_databases_plan(
                static_cast<ReferenceType<ShowDatabasesPlan>>(plan)
            );
        case LogicalPlanKind::ShowCollections:
            return derived().visit_show_collections_plan(
                static_cast<ReferenceType<ShowCollectionsPlan>>(plan)
            );
        case LogicalPlanKind::ShowIndexes:
            return derived().visit_show_indexes_plan(
                static_cast<ReferenceType<ShowIndexesPlan>>(plan)
            );
        case LogicalPlanKind::ShowVectorIndexes:
            return derived().visit_show_vector_indexes_plan(
                static_cast<ReferenceType<ShowVectorIndexesPlan>>(plan)
            );
        case LogicalPlanKind::DescribeCollection:
            return derived().visit_describe_collection_plan(
                static_cast<ReferenceType<DescribeCollectionPlan>>(plan)
            );

        // mutation
        case LogicalPlanKind::Insert:
            return derived().visit_insert_plan(static_cast<ReferenceType<InsertPlan>>(plan));
        case LogicalPlanKind::Update:
            return derived().visit_update_plan(static_cast<ReferenceType<UpdatePlan>>(plan));
        case LogicalPlanKind::Delete:
            return derived().visit_delete_plan(static_cast<ReferenceType<DeletePlan>>(plan));

        // query
        case LogicalPlanKind::Query:
            return derived().visit_query_plan(static_cast<ReferenceType<QueryPlan>>(plan));
        default:
            std::unreachable();
        }
    }

private:
    // 获取派生类引用
    [[nodiscard]]
    Derived & derived() noexcept
    {
        return static_cast<Derived &>(*this);
    }
};

// 常量逻辑计划调度器
template <typename Derived, typename ReturnType>
using ConstLogicalPlanDispatcher = LogicalPlanDispatcher<Derived, ReturnType, true>;

// 可变逻辑计划调度器
template <typename Derived, typename ReturnType>
using MutableLogicalPlanDispatcher = LogicalPlanDispatcher<Derived, ReturnType, false>;

} // namespace litedb::core::logical_planner::plan
