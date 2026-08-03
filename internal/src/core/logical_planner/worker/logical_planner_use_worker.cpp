#include "core/logical_planner/worker/logical_planner_use_worker.hpp"

#include <memory>

#include "core/binder/bound/statement/bound_use_statement.hpp"
#include "core/logical_planner/plan/command/use_plan.hpp"

namespace litedb::core::logical_planner
{

using namespace litedb::core::binder::bound;

std::expected<std::unique_ptr<plan::LogicalPlan>, LogicalPlannerError>
LogicalPlannerUseWorker::plan_use(
    const BoundUseStatement & statement
)
{
    return std::make_unique<plan::UsePlan>(
        statement.database_id()
    );
}

} // namespace litedb::core::logical_planner
