#pragma once

#include <expected>
#include <optional>
#include <vector>

#include "core/common/record.hpp"
#include "core/executor/execution_error.hpp"
#include "core/executor/execution_result.hpp"
#include "core/index/index_engine.hpp"
#include "core/catalog/catalog_viewer.hpp"
#include "core/physical_planner/operator/physical_filter_operator.hpp"
#include "core/physical_planner/operator/physical_index_scan_operator.hpp"
#include "core/physical_planner/operator/physical_limit_operator.hpp"
#include "core/physical_planner/operator/physical_operator.hpp"
#include "core/physical_planner/operator/physical_projection_operator.hpp"
#include "core/physical_planner/operator/physical_seq_scan_operator.hpp"
#include "core/physical_planner/operator/physical_sort_operator.hpp"
#include "core/physical_planner/operator/physical_vector_search_operator.hpp"
#include "core/physical_planner/plan/command/describe_collection_plan.hpp"
#include "core/physical_planner/plan/command/show_collections_plan.hpp"
#include "core/physical_planner/plan/command/show_indexes_plan.hpp"
#include "core/physical_planner/plan/command/show_vector_indexes_plan.hpp"
#include "core/physical_planner/plan/command/use_plan.hpp"
#include "core/physical_planner/plan/mutation/delete_plan.hpp"
#include "core/physical_planner/plan/mutation/insert_plan.hpp"
#include "core/physical_planner/plan/mutation/update_plan.hpp"
#include "core/physical_planner/plan/query/query_plan.hpp"
#include "core/storage/storage_engine.hpp"
#include "core/transaction/transaction_manager.hpp"
#include "core/vindex/vector_index_engine.hpp"

namespace litedb::core::executor::detail
{

/**
 * @brief Materialized row used by the current single-relation executor.
 *
 * source_record is intentionally retained for UPDATE/DELETE identity and for
 * the current BoundColumnRef ordinal contract.  output_values is the visible
 * row produced by the projection stage.
 */
struct MaterializedRow
{
    common::Record source_record;
    std::vector<common::Value> output_values;
};

struct MaterializedResult
{
    std::vector<ExecutionColumn> columns;
    std::vector<MaterializedRow> rows;
};

using PhysicalExecutionResult = std::expected<MaterializedResult, ExecutionError>;
using ExecutionResultExpected = std::expected<ExecutionResult, ExecutionError>;

[[nodiscard]]
PhysicalExecutionResult execute_physical(
    const physical_planner::op::PhysicalOperator & node,
    catalog::CatalogViewer & catalog,
    storage::StorageEngine & storage,
    index::IndexEngine & index_engine,
    vindex::VectorIndexEngine & vector_index_engine
);

[[nodiscard]]
PhysicalExecutionResult execute_scan(
    const physical_planner::op::SeqScanOperator & scan,
    catalog::CatalogViewer & catalog,
    storage::StorageEngine & storage
);

[[nodiscard]]
PhysicalExecutionResult execute_index_scan(
    const physical_planner::op::IndexScanOperator & scan,
    catalog::CatalogViewer & catalog,
    storage::StorageEngine & storage,
    index::IndexEngine & index_engine
);

[[nodiscard]]
PhysicalExecutionResult execute_vector_search(
    const physical_planner::op::VectorSearchOperator & search,
    catalog::CatalogViewer & catalog,
    storage::StorageEngine & storage,
    vindex::VectorIndexEngine & vector_index_engine
);

[[nodiscard]]
PhysicalExecutionResult execute_filter(
    const physical_planner::op::FilterOperator & filter,
    catalog::CatalogViewer & catalog,
    storage::StorageEngine & storage,
    index::IndexEngine & index_engine,
    vindex::VectorIndexEngine & vector_index_engine
);

[[nodiscard]]
PhysicalExecutionResult execute_projection(
    const physical_planner::op::ProjectionOperator & projection,
    catalog::CatalogViewer & catalog,
    storage::StorageEngine & storage,
    index::IndexEngine & index_engine,
    vindex::VectorIndexEngine & vector_index_engine
);

[[nodiscard]]
PhysicalExecutionResult execute_order_by(
    const physical_planner::op::SortOperator & order_by,
    catalog::CatalogViewer & catalog,
    storage::StorageEngine & storage,
    index::IndexEngine & index_engine,
    vindex::VectorIndexEngine & vector_index_engine
);

[[nodiscard]]
PhysicalExecutionResult execute_limit(
    const physical_planner::op::LimitOperator & limit,
    catalog::CatalogViewer & catalog,
    storage::StorageEngine & storage,
    index::IndexEngine & index_engine,
    vindex::VectorIndexEngine & vector_index_engine
);

[[nodiscard]]
ExecutionResultExpected execute_query(
    const physical_planner::plan::QueryPlan & plan,
    catalog::CatalogViewer & catalog,
    storage::StorageEngine & storage,
    index::IndexEngine & index_engine,
    vindex::VectorIndexEngine & vector_index_engine
);

[[nodiscard]]
ExecutionResultExpected
execute_use(const physical_planner::plan::UsePlan & plan, catalog::CatalogViewer & catalog);

[[nodiscard]]
ExecutionResultExpected execute_insert(
    const physical_planner::plan::InsertPlan & plan,
    storage::StorageEngine & storage,
    transaction::TransactionManager & transaction_manager
);

[[nodiscard]]
ExecutionResultExpected execute_delete(
    const physical_planner::plan::DeletePlan & plan,
    catalog::CatalogViewer & catalog,
    storage::StorageEngine & storage,
    index::IndexEngine & index_engine,
    vindex::VectorIndexEngine & vector_index_engine,
    transaction::TransactionManager & transaction_manager
);

[[nodiscard]]
ExecutionResultExpected execute_update(
    const physical_planner::plan::UpdatePlan & plan,
    catalog::CatalogViewer & catalog,
    storage::StorageEngine & storage,
    index::IndexEngine & index_engine,
    vindex::VectorIndexEngine & vector_index_engine,
    transaction::TransactionManager & transaction_manager
);

[[nodiscard]]
ExecutionResultExpected execute_show_databases(catalog::CatalogViewer & catalog);

[[nodiscard]]
ExecutionResultExpected execute_show_collections(
    const physical_planner::plan::ShowCollectionsPlan & plan,
    catalog::CatalogViewer & catalog
);

[[nodiscard]]
ExecutionResultExpected execute_show_indexes(
    const physical_planner::plan::ShowIndexesPlan & plan,
    catalog::CatalogViewer & catalog
);

[[nodiscard]]
ExecutionResultExpected execute_show_vector_indexes(
    const physical_planner::plan::ShowVectorIndexesPlan & plan,
    catalog::CatalogViewer & catalog
);

[[nodiscard]]
ExecutionResultExpected execute_describe_collection(
    const physical_planner::plan::DescribeCollectionPlan & plan,
    catalog::CatalogViewer & catalog
);

} // namespace litedb::core::executor::detail
