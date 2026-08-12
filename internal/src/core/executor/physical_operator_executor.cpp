#include "core/executor/physical_operator_executor.hpp"

namespace litedb::core::executor
{

PhysicalOperatorExecutor::PhysicalOperatorExecutor(
    catalog::CatalogViewer & catalog,
    storage::StorageEngine & storage,
    index::IndexEngine & index_engine,
    vindex::VectorIndexEngine & vector_index_engine
) noexcept
    : catalog_(catalog)
    , storage_(storage)
    , index_engine_(index_engine)
    , vector_index_engine_(vector_index_engine)
{}

detail::PhysicalExecutionResult PhysicalOperatorExecutor::execute(
    const physical_planner::op::PhysicalOperator & node
)
{
    return dispatch_operator(node);
}

detail::PhysicalExecutionResult PhysicalOperatorExecutor::visit_seq_scan_operator(
    const physical_planner::op::SeqScanOperator & scan
)
{
    return detail::execute_scan(scan, catalog_, storage_);
}

detail::PhysicalExecutionResult PhysicalOperatorExecutor::visit_index_scan_operator(
    const physical_planner::op::IndexScanOperator & scan
)
{
    return detail::execute_index_scan(scan, catalog_, storage_, index_engine_);
}

detail::PhysicalExecutionResult PhysicalOperatorExecutor::visit_vector_search_operator(
    const physical_planner::op::VectorSearchOperator & search
)
{
    return detail::execute_vector_search(search, catalog_, storage_, vector_index_engine_);
}

detail::PhysicalExecutionResult PhysicalOperatorExecutor::visit_filter_operator(
    const physical_planner::op::FilterOperator & filter
)
{
    return detail::execute_filter(filter, catalog_, storage_, index_engine_, vector_index_engine_);
}

detail::PhysicalExecutionResult PhysicalOperatorExecutor::visit_projection_operator(
    const physical_planner::op::ProjectionOperator & projection
)
{
    return detail::execute_projection(
        projection,
        catalog_,
        storage_,
        index_engine_,
        vector_index_engine_
    );
}

detail::PhysicalExecutionResult PhysicalOperatorExecutor::visit_sort_operator(
    const physical_planner::op::SortOperator & order_by
)
{
    return detail::execute_order_by(
        order_by,
        catalog_,
        storage_,
        index_engine_,
        vector_index_engine_
    );
}

detail::PhysicalExecutionResult PhysicalOperatorExecutor::visit_limit_operator(
    const physical_planner::op::LimitOperator & limit
)
{
    return detail::execute_limit(limit, catalog_, storage_, index_engine_, vector_index_engine_);
}

namespace detail
{

PhysicalExecutionResult execute_physical(
    const physical_planner::op::PhysicalOperator & node,
    catalog::CatalogViewer & catalog,
    storage::StorageEngine & storage,
    index::IndexEngine & index_engine,
    vindex::VectorIndexEngine & vector_index_engine
)
{
    return PhysicalOperatorExecutor {
        catalog,
        storage,
        index_engine,
        vector_index_engine,
    }
        .execute(node);
}

} // namespace detail

} // namespace litedb::core::executor
