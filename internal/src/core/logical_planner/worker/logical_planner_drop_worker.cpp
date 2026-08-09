#include "core/logical_planner/worker/logical_planner_drop_worker.hpp"

#include "core/binder/bound/statement/bound_drop_collection_statement.hpp"
#include "core/binder/bound/statement/bound_drop_database_statement.hpp"
#include "core/binder/bound/statement/bound_drop_index_statement.hpp"
#include "core/binder/bound/statement/bound_drop_vector_index_statement.hpp"
#include "core/logical_planner/plan/command/drop_database_plan.hpp"
#include "core/logical_planner/plan/command/drop_collection_plan.hpp"
#include "core/logical_planner/plan/command/drop_index_plan.hpp"
#include "core/logical_planner/plan/command/drop_vector_index_plan.hpp"

namespace litedb::core::logical_planner
{

using namespace litedb::core::binder::bound;

std::unique_ptr<plan::LogicalPlan>
LogicalPlannerDropWorker::plan_drop_database(
    const BoundDropDatabaseStatement & statement
)
{
    return std::make_unique<plan::DropDatabasePlan>(
        statement.database_id()
    );
}

std::unique_ptr<plan::LogicalPlan>
LogicalPlannerDropWorker::plan_drop_collection(
    const BoundDropCollectionStatement & statement
)
{
    return std::make_unique<plan::DropCollectionPlan>(
        statement.collection_id()
    );
}

std::unique_ptr<plan::LogicalPlan>
LogicalPlannerDropWorker::plan_drop_index(
    const BoundDropIndexStatement & statement
)
{
    return std::make_unique<plan::DropIndexPlan>(
        statement.index_id()
    );
}

std::unique_ptr<plan::LogicalPlan>
LogicalPlannerDropWorker::plan_drop_vector_index(
    const BoundDropVectorIndexStatement & statement
)
{
    return std::make_unique<plan::DropVectorIndexPlan>(
        statement.vector_index_id()
    );
}

} // namespace litedb::core::logical_planner
