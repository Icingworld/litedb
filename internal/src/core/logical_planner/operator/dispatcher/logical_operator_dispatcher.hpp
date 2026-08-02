#pragma once

#include <utility>

#include "core/logical_planner/operator/logical_plan_operator.hpp"
#include "core/logical_planner/operator/logical_filter_operator.hpp"
#include "core/logical_planner/operator/logical_limit_operator.hpp"
#include "core/logical_planner/operator/logical_order_by_operator.hpp"
#include "core/logical_planner/operator/logical_plan_operator.hpp"
#include "core/logical_planner/operator/logical_projection_operator.hpp"
#include "core/logical_planner/operator/logical_scan_operator.hpp"

namespace litedb::core::logical_planner::op
{

/**
 * @brief 逻辑算子调度器
 * @tparam Derived 派生类
 * @tparam ReturnType 返回类型，默认为 void
 */
template <typename Derived, typename ReturnType = void>
class LogicalOperatorDispatcher
{
protected:
    /**
     * @brief 调度逻辑算子
     * @param op 逻辑算子
     * @return 返回值
     */
    [[nodiscard]]
    ReturnType dispatch_operator(const LogicalPlanOperator & op)
    {
        switch (op.kind()) {
        case LogicalPlanOperatorKind::Scan:
            return derived().visit_scan_operator(
                static_cast<const LogicalScanOperator &>(op)
            );
        case LogicalPlanOperatorKind::Filter:
            return derived().visit_filter_operator(
                static_cast<const LogicalFilterOperator &>(op)
            );
        case LogicalPlanOperatorKind::Projection:
            return derived().visit_projection_operator(
                static_cast<const LogicalProjectionOperator &>(op)
            );
        case LogicalPlanOperatorKind::OrderBy:
            return derived().visit_order_by_operator(
                static_cast<const LogicalOrderByOperator &>(op)
            );
        case LogicalPlanOperatorKind::Limit:
            return derived().visit_limit_operator(
                static_cast<const LogicalLimitOperator &>(op)
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

} // namespace litedb::core::logical_planner::op
