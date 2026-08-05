#include "core/physical_planner/worker/physical_planner_worker.hpp"

#include <cassert>

#include "core/logical_planner/plan/logical_plan.hpp"
#include "core/physical_planner/plan/physical_plan.hpp"
#include "core/physical_planner/worker/physical_command_worker.hpp"
#include "core/physical_planner/worker/physical_mutation_worker.hpp"
#include "core/physical_planner/worker/physical_query_worker.hpp"

namespace litedb::core::physical_planner
{

std::unique_ptr<plan::PhysicalPlan> PhysicalPlannerWorker::plan_statement(
    std::unique_ptr<logical_planner::plan::LogicalPlan> logical_plan
)
{
    assert(logical_plan != nullptr);
    return dispatch_plan(*logical_plan);
}

std::unique_ptr<plan::PhysicalPlan> PhysicalPlannerWorker::visit_use_plan(
    logical_planner::plan::UsePlan & logical_plan
)
{
    return PhysicalCommandWorker().plan_use(logical_plan);
}

std::unique_ptr<plan::PhysicalPlan> PhysicalPlannerWorker::visit_create_database_plan(
    logical_planner::plan::CreateDatabasePlan & logical_plan
)
{
    return PhysicalCommandWorker().plan_create_database(logical_plan);
}

std::unique_ptr<plan::PhysicalPlan> PhysicalPlannerWorker::visit_create_collection_plan(
    logical_planner::plan::CreateCollectionPlan & logical_plan
)
{
    return PhysicalCommandWorker().plan_create_collection(logical_plan);
}

std::unique_ptr<plan::PhysicalPlan> PhysicalPlannerWorker::visit_create_index_plan(
    logical_planner::plan::CreateIndexPlan & logical_plan
)
{
    return PhysicalCommandWorker().plan_create_index(logical_plan);
}

std::unique_ptr<plan::PhysicalPlan> PhysicalPlannerWorker::visit_create_vector_index_plan(
    logical_planner::plan::CreateVectorIndexPlan & logical_plan
)
{
    return PhysicalCommandWorker().plan_create_vector_index(logical_plan);
}

std::unique_ptr<plan::PhysicalPlan> PhysicalPlannerWorker::visit_drop_database_plan(
    logical_planner::plan::DropDatabasePlan & logical_plan
)
{
    return PhysicalCommandWorker().plan_drop_database(logical_plan);
}

std::unique_ptr<plan::PhysicalPlan> PhysicalPlannerWorker::visit_drop_collection_plan(
    logical_planner::plan::DropCollectionPlan & logical_plan
)
{
    return PhysicalCommandWorker().plan_drop_collection(logical_plan);
}

std::unique_ptr<plan::PhysicalPlan> PhysicalPlannerWorker::visit_drop_index_plan(
    logical_planner::plan::DropIndexPlan & logical_plan
)
{
    return PhysicalCommandWorker().plan_drop_index(logical_plan);
}

std::unique_ptr<plan::PhysicalPlan> PhysicalPlannerWorker::visit_drop_vector_index_plan(
    logical_planner::plan::DropVectorIndexPlan & logical_plan
)
{
    return PhysicalCommandWorker().plan_drop_vector_index(logical_plan);
}

std::unique_ptr<plan::PhysicalPlan> PhysicalPlannerWorker::visit_show_databases_plan(
    logical_planner::plan::ShowDatabasesPlan & logical_plan
)
{
    return PhysicalCommandWorker().plan_show_databases(logical_plan);
}

std::unique_ptr<plan::PhysicalPlan> PhysicalPlannerWorker::visit_show_collections_plan(
    logical_planner::plan::ShowCollectionsPlan & logical_plan
)
{
    return PhysicalCommandWorker().plan_show_collections(logical_plan);
}

std::unique_ptr<plan::PhysicalPlan> PhysicalPlannerWorker::visit_show_indexes_plan(
    logical_planner::plan::ShowIndexesPlan & logical_plan
)
{
    return PhysicalCommandWorker().plan_show_indexes(logical_plan);
}

std::unique_ptr<plan::PhysicalPlan> PhysicalPlannerWorker::visit_show_vector_indexes_plan(
    logical_planner::plan::ShowVectorIndexesPlan & logical_plan
)
{
    return PhysicalCommandWorker().plan_show_vector_indexes(logical_plan);
}

std::unique_ptr<plan::PhysicalPlan> PhysicalPlannerWorker::visit_describe_collection_plan(
    logical_planner::plan::DescribeCollectionPlan & logical_plan
)
{
    return PhysicalCommandWorker().plan_describe_collection(logical_plan);
}

std::unique_ptr<plan::PhysicalPlan> PhysicalPlannerWorker::visit_insert_plan(
    logical_planner::plan::InsertPlan & logical_plan
)
{
    return PhysicalMutationWorker(context_).plan_insert(logical_plan);
}

std::unique_ptr<plan::PhysicalPlan> PhysicalPlannerWorker::visit_update_plan(
    logical_planner::plan::UpdatePlan & logical_plan
)
{
    return PhysicalMutationWorker(context_).plan_update(logical_plan);
}

std::unique_ptr<plan::PhysicalPlan> PhysicalPlannerWorker::visit_delete_plan(
    logical_planner::plan::DeletePlan & logical_plan
)
{
    return PhysicalMutationWorker(context_).plan_delete(logical_plan);
}

std::unique_ptr<plan::PhysicalPlan> PhysicalPlannerWorker::visit_query_plan(
    logical_planner::plan::QueryPlan & logical_plan
)
{
    return PhysicalQueryWorker(context_).plan_query(logical_plan);
}

} // namespace litedb::core::physical_planner
