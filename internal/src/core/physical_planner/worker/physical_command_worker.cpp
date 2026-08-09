#include "core/physical_planner/worker/physical_command_worker.hpp"

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

namespace litedb::core::physical_planner
{

std::unique_ptr<plan::PhysicalPlan> PhysicalCommandWorker::plan_use(
    logical_planner::plan::UsePlan & logical_plan
)
{
    return std::make_unique<plan::UsePlan>(logical_plan.database_id());
}

std::unique_ptr<plan::PhysicalPlan> PhysicalCommandWorker::plan_create_database(
    logical_planner::plan::CreateDatabasePlan & logical_plan
)
{
    return std::make_unique<plan::CreateDatabasePlan>(logical_plan.take_database_name());
}

std::unique_ptr<plan::PhysicalPlan> PhysicalCommandWorker::plan_create_collection(
    logical_planner::plan::CreateCollectionPlan & logical_plan
)
{
    return std::make_unique<plan::CreateCollectionPlan>(
        logical_plan.database_id(),
        logical_plan.take_collection_name(),
        logical_plan.take_columns(),
        logical_plan.take_comment()
    );
}

std::unique_ptr<plan::PhysicalPlan> PhysicalCommandWorker::plan_create_index(
    logical_planner::plan::CreateIndexPlan & logical_plan
)
{
    return std::make_unique<plan::CreateIndexPlan>(
        logical_plan.column_id(),
        logical_plan.take_index_name(),
        logical_plan.index_kind(),
        logical_plan.unique()
    );
}

std::unique_ptr<plan::PhysicalPlan> PhysicalCommandWorker::plan_create_vector_index(
    logical_planner::plan::CreateVectorIndexPlan & logical_plan
)
{
    return std::make_unique<plan::CreateVectorIndexPlan>(
        logical_plan.column_id(),
        logical_plan.take_vector_index_name(),
        logical_plan.vector_index_kind(),
        logical_plan.metric(),
        logical_plan.max_neighbors(),
        logical_plan.ef_construction(),
        logical_plan.ef_search_default(),
        logical_plan.random_seed()
    );
}

std::unique_ptr<plan::PhysicalPlan> PhysicalCommandWorker::plan_drop_database(
    logical_planner::plan::DropDatabasePlan & logical_plan
)
{
    return std::make_unique<plan::DropDatabasePlan>(logical_plan.database_id());
}

std::unique_ptr<plan::PhysicalPlan> PhysicalCommandWorker::plan_drop_collection(
    logical_planner::plan::DropCollectionPlan & logical_plan
)
{
    return std::make_unique<plan::DropCollectionPlan>(logical_plan.collection_id());
}

std::unique_ptr<plan::PhysicalPlan> PhysicalCommandWorker::plan_drop_index(
    logical_planner::plan::DropIndexPlan & logical_plan
)
{
    return std::make_unique<plan::DropIndexPlan>(logical_plan.index_id());
}

std::unique_ptr<plan::PhysicalPlan> PhysicalCommandWorker::plan_drop_vector_index(
    logical_planner::plan::DropVectorIndexPlan & logical_plan
)
{
    return std::make_unique<plan::DropVectorIndexPlan>(logical_plan.vector_index_id());
}

std::unique_ptr<plan::PhysicalPlan> PhysicalCommandWorker::plan_show_databases(
    logical_planner::plan::ShowDatabasesPlan &
)
{
    return std::make_unique<plan::ShowDatabasesPlan>();
}

std::unique_ptr<plan::PhysicalPlan> PhysicalCommandWorker::plan_show_collections(
    logical_planner::plan::ShowCollectionsPlan & logical_plan
)
{
    return std::make_unique<plan::ShowCollectionsPlan>(logical_plan.database_id());
}

std::unique_ptr<plan::PhysicalPlan> PhysicalCommandWorker::plan_show_indexes(
    logical_planner::plan::ShowIndexesPlan & logical_plan
)
{
    return std::make_unique<plan::ShowIndexesPlan>(logical_plan.collection_id());
}

std::unique_ptr<plan::PhysicalPlan> PhysicalCommandWorker::plan_show_vector_indexes(
    logical_planner::plan::ShowVectorIndexesPlan & logical_plan
)
{
    return std::make_unique<plan::ShowVectorIndexesPlan>(logical_plan.collection_id());
}

std::unique_ptr<plan::PhysicalPlan> PhysicalCommandWorker::plan_describe_collection(
    logical_planner::plan::DescribeCollectionPlan & logical_plan
)
{
    return std::make_unique<plan::DescribeCollectionPlan>(logical_plan.collection_id());
}

} // namespace litedb::core::physical_planner
