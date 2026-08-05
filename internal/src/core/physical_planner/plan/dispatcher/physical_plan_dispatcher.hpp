#pragma once

#include <utility>
#include <type_traits>

#include "core/physical_planner/plan/command/create_collection_plan.hpp"
#include "core/physical_planner/plan/command/create_database_plan.hpp"
#include "core/physical_planner/plan/command/create_index_plan.hpp"
#include "core/physical_planner/plan/command/create_vector_index_plan.hpp"
#include "core/physical_planner/plan/command/describe_collection_plan.hpp"
#include "core/physical_planner/plan/command/drop_collection_plan.hpp"
#include "core/physical_planner/plan/command/drop_database_plan.hpp"
#include "core/physical_planner/plan/command/drop_index_plan.hpp"
#include "core/physical_planner/plan/command/drop_vector_index_plan.hpp"
#include "core/physical_planner/plan/command/show_collections_plan.hpp"
#include "core/physical_planner/plan/command/show_databases_plan.hpp"
#include "core/physical_planner/plan/command/show_indexes_plan.hpp"
#include "core/physical_planner/plan/command/show_vector_indexes_plan.hpp"
#include "core/physical_planner/plan/command/use_plan.hpp"
#include "core/physical_planner/plan/mutation/delete_plan.hpp"
#include "core/physical_planner/plan/mutation/insert_plan.hpp"
#include "core/physical_planner/plan/mutation/update_plan.hpp"
#include "core/physical_planner/plan/physical_plan.hpp"
#include "core/physical_planner/plan/query/query_plan.hpp"

namespace litedb::core::physical_planner::plan
{

/**
 * @brief 物理计划调度器
 * @tparam Derived 派生类
 * @tparam ReturnType 返回类型
 * @tparam IsConst 是否为常量
 */
template <
    typename Derived,
    typename ReturnType,
    bool IsConst
>
class PhysicalPlanDispatcher
{
protected:
    /**
     * @brief 引用类型
     */
    template <typename T>
    using ReferenceType = std::conditional_t<
        IsConst,
        const T &,
        T &
    >;

protected:
    /**
     * @brief 调度物理计划
     * @param plan 物理计划
     * @return 返回值
     */
    [[nodiscard]]
    ReturnType dispatch_plan(ReferenceType<PhysicalPlan> plan)
    {
        switch (plan.kind()) {
        case PhysicalPlanKind::Use:
            return derived().visit_use_plan(
                static_cast<ReferenceType<UsePlan>>(plan)
            );
        case PhysicalPlanKind::CreateDatabase:
            return derived().visit_create_database_plan(
                static_cast<ReferenceType<CreateDatabasePlan>>(plan)
            );
        case PhysicalPlanKind::CreateCollection:
            return derived().visit_create_collection_plan(
                static_cast<ReferenceType<CreateCollectionPlan>>(plan)
            );
        case PhysicalPlanKind::CreateIndex:
            return derived().visit_create_index_plan(
                static_cast<ReferenceType<CreateIndexPlan>>(plan)
            );
        case PhysicalPlanKind::CreateVectorIndex:
            return derived().visit_create_vector_index_plan(
                static_cast<ReferenceType<CreateVectorIndexPlan>>(plan)
            );
        case PhysicalPlanKind::DropDatabase:
            return derived().visit_drop_database_plan(
                static_cast<ReferenceType<DropDatabasePlan>>(plan)
            );
        case PhysicalPlanKind::DropCollection:
            return derived().visit_drop_collection_plan(
                static_cast<ReferenceType<DropCollectionPlan>>(plan)
            );
        case PhysicalPlanKind::DropIndex:
            return derived().visit_drop_index_plan(
                static_cast<ReferenceType<DropIndexPlan>>(plan)
            );
        case PhysicalPlanKind::DropVectorIndex:
            return derived().visit_drop_vector_index_plan(
                static_cast<ReferenceType<DropVectorIndexPlan>>(plan)
            );
        case PhysicalPlanKind::ShowDatabases:
            return derived().visit_show_databases_plan(
                static_cast<ReferenceType<ShowDatabasesPlan>>(plan)
            );
        case PhysicalPlanKind::ShowCollections:
            return derived().visit_show_collections_plan(
                static_cast<ReferenceType<ShowCollectionsPlan>>(plan)
            );
        case PhysicalPlanKind::ShowIndexes:
            return derived().visit_show_indexes_plan(
                static_cast<ReferenceType<ShowIndexesPlan>>(plan)
            );
        case PhysicalPlanKind::ShowVectorIndexes:
            return derived().visit_show_vector_indexes_plan(
                static_cast<ReferenceType<ShowVectorIndexesPlan>>(plan)
            );
        case PhysicalPlanKind::DescribeCollection:
            return derived().visit_describe_collection_plan(
                static_cast<ReferenceType<DescribeCollectionPlan>>(plan)
            );
        case PhysicalPlanKind::Insert:
            return derived().visit_insert_plan(
                static_cast<ReferenceType<InsertPlan>>(plan)
            );
        case PhysicalPlanKind::Update:
            return derived().visit_update_plan(
                static_cast<ReferenceType<UpdatePlan>>(plan)
            );
        case PhysicalPlanKind::Delete:
            return derived().visit_delete_plan(
                static_cast<ReferenceType<DeletePlan>>(plan)
            );
        case PhysicalPlanKind::Query:
            return derived().visit_query_plan(
                static_cast<ReferenceType<QueryPlan>>(plan)
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

/**
 * @brief 常量物理计划调度器
 * @tparam Derived 派生类
 * @tparam ReturnType 返回类型
 */
template <typename Derived, typename ReturnType>
using ConstPhysicalPlanDispatcher = PhysicalPlanDispatcher<
    Derived,
    ReturnType,
    true
>;

/**
 * @brief 可变物理计划调度器
 * @tparam Derived 派生类
 * @tparam ReturnType 返回类型
 */
template <typename Derived, typename ReturnType>
using MutablePhysicalPlanDispatcher = PhysicalPlanDispatcher<
    Derived,
    ReturnType,
    false
>;

} // namespace litedb::core::physical_planner::plan
