#include "core/logical_planner/worker/logical_planner_update_worker.hpp"

#include "core/binder/bound/statement/bound_update_statement.hpp"
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
    // 生成 Scan 算子
    auto scan_operator = std::make_unique<op::LogicalScanOperator>(
        statement.collection_id()
    );

    return std::make_unique<plan::UpdatePlan>(
        statement.collection_id(),
        statement.take_assignments(),
        std::move(scan_operator)
    );
}

} // namespace litedb::core::logical_planner
