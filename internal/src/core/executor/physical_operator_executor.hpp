#pragma once

#include "core/executor/executor_detail.hpp"
#include "core/physical_planner/operator/dispatcher/physical_operator_dispatcher.hpp"

namespace litedb::core::executor
{

class PhysicalOperatorExecutor final
    : private physical_planner::op::
          ConstPhysicalOperatorDispatcher<PhysicalOperatorExecutor, detail::PhysicalExecutionResult>
{
    friend physical_planner::op::
        ConstPhysicalOperatorDispatcher<PhysicalOperatorExecutor, detail::PhysicalExecutionResult>;

public:
    PhysicalOperatorExecutor(
        catalog::CatalogViewer & catalog,
        storage::StorageEngine & storage,
        index::IndexEngine & index_engine,
        vindex::VectorIndexEngine & vector_index_engine
    ) noexcept;

    [[nodiscard]]
    detail::PhysicalExecutionResult execute(const physical_planner::op::PhysicalOperator & node);

private:
    [[nodiscard]]
    detail::PhysicalExecutionResult visit_seq_scan_operator(
        const physical_planner::op::SeqScanOperator & scan
    );

    [[nodiscard]]
    detail::PhysicalExecutionResult visit_index_scan_operator(
        const physical_planner::op::IndexScanOperator & scan
    );

    [[nodiscard]]
    detail::PhysicalExecutionResult visit_vector_search_operator(
        const physical_planner::op::VectorSearchOperator & search
    );

    [[nodiscard]]
    detail::PhysicalExecutionResult visit_filter_operator(
        const physical_planner::op::FilterOperator & filter
    );

    [[nodiscard]]
    detail::PhysicalExecutionResult visit_projection_operator(
        const physical_planner::op::ProjectionOperator & projection
    );

    [[nodiscard]]
    detail::PhysicalExecutionResult visit_sort_operator(
        const physical_planner::op::SortOperator & order_by
    );

    [[nodiscard]]
    detail::PhysicalExecutionResult visit_limit_operator(
        const physical_planner::op::LimitOperator & limit
    );

private:
    catalog::CatalogViewer & catalog_;
    storage::StorageEngine & storage_;
    index::IndexEngine & index_engine_;
    vindex::VectorIndexEngine & vector_index_engine_;
};

} // namespace litedb::core::executor
