#include "core/logical_planner/worker/logical_planner_update_worker.hpp"

#include "core/binder/bound/statement/bound_update_statement.hpp"
#include "core/logical_planner/operator/logical_filter_operator.hpp"
#include "core/logical_planner/operator/logical_plan_operator.hpp"
#include "core/logical_planner/operator/logical_scan_operator.hpp"
#include "core/logical_planner/plan/mutation/update_plan.hpp"

namespace litedb::core::logical_planner
{

using namespace litedb::core::binder::bound;

std::expected<std::unique_ptr<plan::LogicalPlan>, LogicalPlannerError>
LogicalPlannerUpdateWorker::plan_update(
    BoundUpdateStatement & statement
)
{
    std::unique_ptr<op::LogicalPlanOperator> root_operator =
        std::make_unique<op::LogicalScanOperator>(
        statement.collection_id()
    );

    auto where = statement.take_where();
    if (where != nullptr) {
        root_operator = std::make_unique<op::LogicalFilterOperator>(
            std::move(root_operator),
            std::move(where)
        );
    }

    return std::make_unique<plan::UpdatePlan>(
        statement.collection_id(),
        statement.take_assignments(),
        std::move(root_operator)
    );
}

} // namespace litedb::core::logical_planner
