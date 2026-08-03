#include "core/logical_planner/worker/logical_planner_select_worker.hpp"

#include <memory>
#include <utility>

#include "core/binder/bound/statement/bound_select_statement.hpp"
#include "core/logical_planner/operator/logical_filter_operator.hpp"
#include "core/logical_planner/operator/logical_limit_operator.hpp"
#include "core/logical_planner/operator/logical_order_by_operator.hpp"
#include "core/logical_planner/operator/logical_plan_operator.hpp"
#include "core/logical_planner/operator/logical_projection_operator.hpp"
#include "core/logical_planner/operator/logical_scan_operator.hpp"
#include "core/logical_planner/plan/query/query_plan.hpp"

namespace litedb::core::logical_planner
{

using namespace litedb::core::binder::bound;

std::expected<std::unique_ptr<plan::LogicalPlan>, LogicalPlannerError>
LogicalPlannerSelectWorker::plan_select(
    BoundSelectStatement & statement
)
{
    // 自底向上构建逻辑计划

    // 创建 Scan 算子
    std::unique_ptr<op::LogicalPlanOperator> current =
        std::make_unique<op::LogicalScanOperator>(statement.collection_id());

    // 将 Scan 算子挂在 Filter 算子上
    auto where = statement.take_where();
    if (where != nullptr) {
        current = std::make_unique<op::LogicalFilterOperator>(
            std::move(current),
            std::move(where)
        );
    }

    // 将 Filter 算子挂在 Projection 算子上
    current = std::make_unique<op::LogicalProjectionOperator>(
        std::move(current),
        statement.take_projections()
    );

    // 将 Projection 算子挂在 Order By 算子上
    auto order_by = statement.take_order_by();
    if (!order_by.empty()) {
        current = std::make_unique<op::LogicalOrderByOperator>(
            std::move(current),
            std::move(order_by)
        );
    }

    // 将 Order By 算子挂在 Limit 算子上
    if (statement.limit().has_value() || statement.offset().has_value()) {
        current = std::make_unique<op::LogicalLimitOperator>(
            std::move(current),
            statement.limit(),
            statement.offset()
        );
    }

    return std::make_unique<plan::QueryPlan>(std::move(current));
}

} // namespace litedb::core::logical_planner
