#include "core/logical_planner/worker/logical_planner_worker.hpp"

#include "core/logical_planner/plan/logical_plan.hpp"
#include "core/logical_planner/worker/logical_planner_create_worker.hpp"
#include "core/logical_planner/worker/logical_planner_delete_worker.hpp"
#include "core/logical_planner/worker/logical_planner_describe_worker.hpp"
#include "core/logical_planner/worker/logical_planner_drop_worker.hpp"
#include "core/logical_planner/worker/logical_planner_insert_worker.hpp"
#include "core/logical_planner/worker/logical_planner_select_worker.hpp"
#include "core/logical_planner/worker/logical_planner_show_worker.hpp"
#include "core/logical_planner/worker/logical_planner_update_worker.hpp"
#include "core/logical_planner/worker/logical_planner_use_worker.hpp"

namespace litedb::core::logical_planner
{

using namespace litedb::core::binder::bound;

std::unique_ptr<plan::LogicalPlan>
LogicalPlannerWorker::plan_statement(
    BoundStatement & statement
)
{
    return dispatch_statement(statement);
}

std::unique_ptr<plan::LogicalPlan>
LogicalPlannerWorker::visit_create_database_statement(
    const BoundCreateDatabaseStatement & statement
)
{
    return LogicalPlannerCreateWorker().plan_create_database(statement);
}

std::unique_ptr<plan::LogicalPlan>
LogicalPlannerWorker::visit_create_collection_statement(
    const BoundCreateCollectionStatement & statement
)
{
    return LogicalPlannerCreateWorker().plan_create_collection(statement);
}

std::unique_ptr<plan::LogicalPlan>
LogicalPlannerWorker::visit_create_index_statement(
    const BoundCreateIndexStatement & statement
)
{
    return LogicalPlannerCreateWorker().plan_create_index(statement);
}

std::unique_ptr<plan::LogicalPlan>
LogicalPlannerWorker::visit_create_vector_index_statement(
    const BoundCreateVectorIndexStatement & statement
)
{
    return LogicalPlannerCreateWorker().plan_create_vector_index(statement);
}

std::unique_ptr<plan::LogicalPlan>
LogicalPlannerWorker::visit_delete_statement(
    BoundDeleteStatement & statement
)
{
    return LogicalPlannerDeleteWorker().plan_delete(statement);
}

std::unique_ptr<plan::LogicalPlan>
LogicalPlannerWorker::visit_describe_collection_statement(
    const BoundDescribeCollectionStatement & statement
)
{
    return LogicalPlannerDescribeWorker().plan_describe_collection(statement);
}

std::unique_ptr<plan::LogicalPlan>
LogicalPlannerWorker::visit_drop_database_statement(
    const BoundDropDatabaseStatement & statement
)
{
    return LogicalPlannerDropWorker().plan_drop_database(statement);
}

std::unique_ptr<plan::LogicalPlan>
LogicalPlannerWorker::visit_drop_collection_statement(
    const BoundDropCollectionStatement & statement
)
{
    return LogicalPlannerDropWorker().plan_drop_collection(statement);
}

std::unique_ptr<plan::LogicalPlan>
LogicalPlannerWorker::visit_drop_index_statement(
    const BoundDropIndexStatement & statement
)
{
    return LogicalPlannerDropWorker().plan_drop_index(statement);
}

std::unique_ptr<plan::LogicalPlan>
LogicalPlannerWorker::visit_drop_vector_index_statement(
    const BoundDropVectorIndexStatement & statement
)
{
    return LogicalPlannerDropWorker().plan_drop_vector_index(statement);
}

std::unique_ptr<plan::LogicalPlan>
LogicalPlannerWorker::visit_insert_statement(
    BoundInsertStatement & statement
)
{
    return LogicalPlannerInsertWorker().plan_insert(statement);
}

std::unique_ptr<plan::LogicalPlan>
LogicalPlannerWorker::visit_select_statement(
    BoundSelectStatement & statement
)
{
    return LogicalPlannerSelectWorker().plan_select(statement);
}

std::unique_ptr<plan::LogicalPlan>
LogicalPlannerWorker::visit_show_databases_statement(
    const BoundShowDatabasesStatement & statement
)
{
    return LogicalPlannerShowWorker().plan_show_databases(statement);
}

std::unique_ptr<plan::LogicalPlan>
LogicalPlannerWorker::visit_show_collections_statement(
    const BoundShowCollectionsStatement & statement
)
{
    return LogicalPlannerShowWorker().plan_show_collections(statement);
}

std::unique_ptr<plan::LogicalPlan>
LogicalPlannerWorker::visit_show_indexes_statement(
    const BoundShowIndexesStatement & statement
)
{
    return LogicalPlannerShowWorker().plan_show_indexes(statement);
}

std::unique_ptr<plan::LogicalPlan>
LogicalPlannerWorker::visit_show_vector_indexes_statement(
    const BoundShowVectorIndexesStatement & statement
)
{
    return LogicalPlannerShowWorker().plan_show_vector_indexes(statement);
}

std::unique_ptr<plan::LogicalPlan>
LogicalPlannerWorker::visit_update_statement(
    BoundUpdateStatement & statement
)
{
    return LogicalPlannerUpdateWorker().plan_update(statement);
}

std::unique_ptr<plan::LogicalPlan>
LogicalPlannerWorker::visit_use_statement(
    const BoundUseStatement & statement
)
{
    return LogicalPlannerUseWorker().plan_use(statement);
}

} // namespace litedb::core::logical_planner
