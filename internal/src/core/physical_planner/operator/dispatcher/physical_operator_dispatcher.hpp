#pragma once

#include <utility>
#include <type_traits>

#include "core/physical_planner/operator/physical_filter_operator.hpp"
#include "core/physical_planner/operator/physical_index_scan_operator.hpp"
#include "core/physical_planner/operator/physical_limit_operator.hpp"
#include "core/physical_planner/operator/physical_operator.hpp"
#include "core/physical_planner/operator/physical_projection_operator.hpp"
#include "core/physical_planner/operator/physical_seq_scan_operator.hpp"
#include "core/physical_planner/operator/physical_sort_operator.hpp"
#include "core/physical_planner/operator/physical_vector_search_operator.hpp"

namespace litedb::core::physical_planner::op
{

// 物理算子调度器
template <
    typename Derived,
    typename ReturnType,
    bool IsConst
>
class PhysicalOperatorDispatcher
{
protected:
    // 引用类型
    template <typename T>
    using ReferenceType = std::conditional_t<
        IsConst,
        const T &,
        T &
    >;

protected:
    // 调度物理算子
    [[nodiscard]]
    ReturnType dispatch_operator(ReferenceType<PhysicalOperator> op)
    {
        switch (op.kind()) {
        case PhysicalOperatorKind::SeqScan:
            return derived().visit_seq_scan_operator(
                static_cast<ReferenceType<SeqScanOperator>>(op)
            );
        case PhysicalOperatorKind::IndexScan:
            return derived().visit_index_scan_operator(
                static_cast<ReferenceType<IndexScanOperator>>(op)
            );
        case PhysicalOperatorKind::VectorSearch:
            return derived().visit_vector_search_operator(
                static_cast<ReferenceType<VectorSearchOperator>>(op)
            );
        case PhysicalOperatorKind::Filter:
            return derived().visit_filter_operator(
                static_cast<ReferenceType<FilterOperator>>(op)
            );
        case PhysicalOperatorKind::Projection:
            return derived().visit_projection_operator(
                static_cast<ReferenceType<ProjectionOperator>>(op)
            );
        case PhysicalOperatorKind::Sort:
            return derived().visit_sort_operator(
                static_cast<ReferenceType<SortOperator>>(op)
            );
        case PhysicalOperatorKind::Limit:
            return derived().visit_limit_operator(
                static_cast<ReferenceType<LimitOperator>>(op)
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

// 常量物理算子调度器
template <typename Derived, typename ReturnType>
using ConstPhysicalOperatorDispatcher = PhysicalOperatorDispatcher<
    Derived,
    ReturnType,
    true
>;

// 可变物理算子调度器
template <typename Derived, typename ReturnType>
using MutablePhysicalOperatorDispatcher = PhysicalOperatorDispatcher<
    Derived,
    ReturnType,
    false
>;

} // namespace litedb::core::physical_planner::op
