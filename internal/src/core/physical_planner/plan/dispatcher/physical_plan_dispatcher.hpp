#pragma once

#include <type_traits>

#include "core/physical_planner/plan/command/command_plans.hpp"
#include "core/physical_planner/plan/mutation/delete_plan.hpp"
#include "core/physical_planner/plan/mutation/insert_plan.hpp"
#include "core/physical_planner/plan/mutation/update_plan.hpp"
#include "core/physical_planner/plan/physical_plan.hpp"
#include "core/physical_planner/plan/query/query_plan.hpp"

namespace litedb::core::physical_planner::plan
{

template <typename Derived, typename ReturnType, bool IsConst>
class PhysicalPlanDispatcher
{
protected:
    template <typename T>
    using ReferenceType = std::conditional_t<IsConst, const T &, T &>;

    [[nodiscard]] ReturnType dispatch_plan(ReferenceType<PhysicalPlan> plan)
    {
        switch (plan.kind()) {
        case PhysicalPlanKind::Use:
            return derived().visit_use_plan(static_cast<ReferenceType<UsePlan>>(plan));
        case PhysicalPlanKind::CreateDatabase:
            return derived().visit_create_database_plan(static_cast<ReferenceType<CreateDatabasePlan>>(plan));
        case PhysicalPlanKind::CreateCollection:
            return derived().visit_create_collection_plan(static_cast<ReferenceType<CreateCollectionPlan>>(plan));
        case PhysicalPlanKind::CreateIndex:
            return derived().visit_create_index_plan(static_cast<ReferenceType<CreateIndexPlan>>(plan));
        case PhysicalPlanKind::CreateVectorIndex:
            return derived().visit_create_vector_index_plan(static_cast<ReferenceType<CreateVectorIndexPlan>>(plan));
        case PhysicalPlanKind::DropDatabase:
            return derived().visit_drop_database_plan(static_cast<ReferenceType<DropDatabasePlan>>(plan));
        case PhysicalPlanKind::DropCollection:
            return derived().visit_drop_collection_plan(static_cast<ReferenceType<DropCollectionPlan>>(plan));
        case PhysicalPlanKind::DropIndex:
            return derived().visit_drop_index_plan(static_cast<ReferenceType<DropIndexPlan>>(plan));
        case PhysicalPlanKind::DropVectorIndex:
            return derived().visit_drop_vector_index_plan(static_cast<ReferenceType<DropVectorIndexPlan>>(plan));
        case PhysicalPlanKind::ShowDatabases:
            return derived().visit_show_databases_plan(static_cast<ReferenceType<ShowDatabasesPlan>>(plan));
        case PhysicalPlanKind::ShowCollections:
            return derived().visit_show_collections_plan(static_cast<ReferenceType<ShowCollectionsPlan>>(plan));
        case PhysicalPlanKind::ShowIndexes:
            return derived().visit_show_indexes_plan(static_cast<ReferenceType<ShowIndexesPlan>>(plan));
        case PhysicalPlanKind::ShowVectorIndexes:
            return derived().visit_show_vector_indexes_plan(static_cast<ReferenceType<ShowVectorIndexesPlan>>(plan));
        case PhysicalPlanKind::DescribeCollection:
            return derived().visit_describe_collection_plan(static_cast<ReferenceType<DescribeCollectionPlan>>(plan));
        case PhysicalPlanKind::Insert:
            return derived().visit_insert_plan(static_cast<ReferenceType<InsertPlan>>(plan));
        case PhysicalPlanKind::Update:
            return derived().visit_update_plan(static_cast<ReferenceType<UpdatePlan>>(plan));
        case PhysicalPlanKind::Delete:
            return derived().visit_delete_plan(static_cast<ReferenceType<DeletePlan>>(plan));
        case PhysicalPlanKind::Query:
            return derived().visit_query_plan(static_cast<ReferenceType<QueryPlan>>(plan));
        default:
            std::unreachable();
        }
    }

private:
    [[nodiscard]] Derived & derived() noexcept
    {
        return static_cast<Derived &>(*this);
    }
};

template <typename Derived, typename ReturnType>
using ConstPhysicalPlanDispatcher = PhysicalPlanDispatcher<Derived, ReturnType, true>;

template <typename Derived, typename ReturnType>
using MutablePhysicalPlanDispatcher = PhysicalPlanDispatcher<Derived, ReturnType, false>;

} // namespace litedb::core::physical_planner::plan
