#pragma once

#include <type_traits>
#include <utility>

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

template <typename Derived, typename ReturnType, bool IsConst>
class PhysicalOperatorDispatcher
{
protected:
    template <typename T>
    using ReferenceType = std::conditional_t<IsConst, const T &, T &>;

    [[nodiscard]] ReturnType dispatch_operator(ReferenceType<PhysicalOperator> op)
    {
        switch (op.kind()) {
        case PhysicalOperatorKind::SeqScan:
            return derived().visit_seq_scan_operator(static_cast<ReferenceType<SeqScanOperator>>(op));
        case PhysicalOperatorKind::IndexScan:
            return derived().visit_index_scan_operator(static_cast<ReferenceType<IndexScanOperator>>(op));
        case PhysicalOperatorKind::VectorSearch:
            return derived().visit_vector_search_operator(static_cast<ReferenceType<VectorSearchOperator>>(op));
        case PhysicalOperatorKind::Filter:
            return derived().visit_filter_operator(static_cast<ReferenceType<FilterOperator>>(op));
        case PhysicalOperatorKind::Projection:
            return derived().visit_projection_operator(static_cast<ReferenceType<ProjectionOperator>>(op));
        case PhysicalOperatorKind::Sort:
            return derived().visit_sort_operator(static_cast<ReferenceType<SortOperator>>(op));
        case PhysicalOperatorKind::Limit:
            return derived().visit_limit_operator(static_cast<ReferenceType<LimitOperator>>(op));
        default:
            std::unreachable();
        }
    }

private:
    [[nodiscard]] Derived & derived() noexcept
    {
        return static_cast<Derived &>(*this);
    }
};

template <typename Derived, typename ReturnType>
using ConstPhysicalOperatorDispatcher = PhysicalOperatorDispatcher<Derived, ReturnType, true>;

template <typename Derived, typename ReturnType>
using MutablePhysicalOperatorDispatcher = PhysicalOperatorDispatcher<Derived, ReturnType, false>;

} // namespace litedb::core::physical_planner::op
