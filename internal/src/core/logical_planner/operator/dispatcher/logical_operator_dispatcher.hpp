#pragma once

#include <type_traits>
#include <utility>

#include "core/logical_planner/operator/logical_filter_operator.hpp"
#include "core/logical_planner/operator/logical_limit_operator.hpp"
#include "core/logical_planner/operator/logical_order_by_operator.hpp"
#include "core/logical_planner/operator/logical_plan_operator.hpp"
#include "core/logical_planner/operator/logical_projection_operator.hpp"
#include "core/logical_planner/operator/logical_scan_operator.hpp"

namespace litedb::core::logical_planner::op
{

// 逻辑算子调度器
template <typename Derived, typename ReturnType, bool IsConst>
class LogicalOperatorDispatcher
{
protected:
    // 引用类型
    template <typename T>
    using ReferenceType = std::conditional_t<IsConst, const T &, T &>;

protected:
    // 调度逻辑算子
    [[nodiscard]]
    ReturnType dispatch_operator(ReferenceType<LogicalPlanOperator> op)
    {
        switch (op.kind()) {
        case LogicalPlanOperatorKind::Scan:
            return derived().visit_scan_operator(
                static_cast<ReferenceType<LogicalScanOperator>>(op)
            );
        case LogicalPlanOperatorKind::Filter:
            return derived().visit_filter_operator(
                static_cast<ReferenceType<LogicalFilterOperator>>(op)
            );
        case LogicalPlanOperatorKind::Projection:
            return derived().visit_projection_operator(
                static_cast<ReferenceType<LogicalProjectionOperator>>(op)
            );
        case LogicalPlanOperatorKind::OrderBy:
            return derived().visit_order_by_operator(
                static_cast<ReferenceType<LogicalOrderByOperator>>(op)
            );
        case LogicalPlanOperatorKind::Limit:
            return derived().visit_limit_operator(
                static_cast<ReferenceType<LogicalLimitOperator>>(op)
            );
        default:
            std::unreachable();
        }
    }

private:
    // 获取派生类引用
    [[nodiscard]]
    Derived & derived() noexcept
    {
        return static_cast<Derived &>(*this);
    }
};

// 常量逻辑算子调度器
template <typename Derived, typename ReturnType>
using ConstLogicalOperatorDispatcher = LogicalOperatorDispatcher<Derived, ReturnType, true>;

// 可变逻辑算子调度器
template <typename Derived, typename ReturnType>
using MutableLogicalOperatorDispatcher = LogicalOperatorDispatcher<Derived, ReturnType, false>;

} // namespace litedb::core::logical_planner::op
