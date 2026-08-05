#include "core/executor/executor.hpp"

#include <algorithm>
#include <expected>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "core/binder/bound/expression/bound_column_ref_expression.hpp"
#include "core/meta/meta.hpp"
#include "core/evaluator/expression_evaluator.hpp"
#include "core/index/index_engine.hpp"
#include "core/index/scalar_index.hpp"
#include "core/physical_planner/operator/physical_filter_operator.hpp"
#include "core/physical_planner/operator/physical_index_scan_operator.hpp"
#include "core/physical_planner/operator/physical_limit_operator.hpp"
#include "core/physical_planner/operator/physical_operator.hpp"
#include "core/physical_planner/operator/physical_projection_operator.hpp"
#include "core/physical_planner/operator/physical_seq_scan_operator.hpp"
#include "core/physical_planner/operator/physical_sort_operator.hpp"
#include "core/physical_planner/operator/physical_vector_search_operator.hpp"
#include "core/physical_planner/plan/command/create_collection_plan.hpp"
#include "core/physical_planner/plan/command/create_database_plan.hpp"
#include "core/physical_planner/plan/command/create_index_plan.hpp"
#include "core/physical_planner/plan/command/create_vector_index_plan.hpp"
#include "core/physical_planner/plan/command/describe_collection_plan.hpp"
#include "core/physical_planner/plan/command/drop_collection_plan.hpp"
#include "core/physical_planner/plan/command/drop_database_plan.hpp"
#include "core/physical_planner/plan/command/drop_index_plan.hpp"
#include "core/physical_planner/plan/command/drop_vector_index_plan.hpp"
#include "core/physical_planner/plan/command/show_collections_plan.hpp"
#include "core/physical_planner/plan/command/show_databases_plan.hpp"
#include "core/physical_planner/plan/command/show_indexes_plan.hpp"
#include "core/physical_planner/plan/command/show_vector_indexes_plan.hpp"
#include "core/physical_planner/plan/command/use_plan.hpp"
#include "core/physical_planner/plan/mutation/insert_plan.hpp"
#include "core/physical_planner/plan/query/query_plan.hpp"
#include "core/physical_planner/plan/mutation/update_plan.hpp"
#include "core/physical_planner/plan/mutation/delete_plan.hpp"
#include "core/physical_planner/plan/physical_plan.hpp"
#include "core/storage/schema_loader.hpp"
#include "core/transaction/transaction_manager.hpp"

namespace litedb::core::executor
{

namespace
{

using binder::bound::BoundExpression;
using common::LogicalType;
using common::LogicalTypeId;
using physical_planner::plan::CreateCollectionPlan;
using physical_planner::plan::CreateDatabasePlan;
using physical_planner::plan::CreateIndexPlan;
using physical_planner::plan::CreateVectorIndexPlan;
using physical_planner::plan::DeletePlan;
using physical_planner::plan::DescribeCollectionPlan;
using physical_planner::plan::DropCollectionPlan;
using physical_planner::plan::DropDatabasePlan;
using physical_planner::plan::DropIndexPlan;
using physical_planner::plan::DropVectorIndexPlan;
using physical_planner::op::FilterOperator;
using physical_planner::op::IndexLookupKind;
using physical_planner::op::IndexScanOperator;
using physical_planner::plan::InsertPlan;
using physical_planner::op::LimitOperator;
using physical_planner::op::PhysicalOperator;
using physical_planner::op::PhysicalOperatorKind;
using physical_planner::op::ProjectionOperator;
using physical_planner::plan::QueryPlan;
using physical_planner::op::SeqScanOperator;
using physical_planner::plan::ShowCollectionsPlan;
using physical_planner::plan::ShowIndexesPlan;
using physical_planner::plan::ShowVectorIndexesPlan;
using physical_planner::op::SortOperator;
using physical_planner::plan::PhysicalPlan;
using physical_planner::plan::PhysicalPlanKind;
using physical_planner::plan::UpdatePlan;
using physical_planner::plan::UsePlan;
using physical_planner::op::VectorSearchOperator;

struct PipelineRow
{
    common::Record source_record;
    std::vector<common::Value> output_values;
};

struct PipelineResult
{
    std::vector<ExecutionColumn> columns;
    std::vector<PipelineRow> rows;
};

[[nodiscard]]
LogicalType type(LogicalTypeId id, std::optional<std::size_t> parameter = std::nullopt)
{
    return LogicalType {id, parameter};
}

[[nodiscard]]
ExecutionError make_error(ExecutionErrorCode code, std::string message)
{
    return ExecutionError {code, message};
}

[[nodiscard]]
ExecutionError make_error(
    ExecutionErrorCode code,
    std::string message,
    error::Error cause
)
{
    return ExecutionError {code, message, std::move(cause)};
}

[[nodiscard]]
ExecutionError from_meta_error(meta::MetaError error)
{
    auto message = error.message();
    return make_error(ExecutionErrorCode::MetaError, std::move(message), std::move(error));
}

[[nodiscard]]
ExecutionError from_schema_error(storage::SchemaLoadError error)
{
    auto message = error.message();
    return make_error(ExecutionErrorCode::SchemaError, std::move(message), std::move(error));
}

[[nodiscard]]
ExecutionError from_storage_error(storage::StorageError error)
{
    auto message = error.message();
    return make_error(ExecutionErrorCode::StorageError, std::move(message), std::move(error));
}

[[nodiscard]]
ExecutionError from_index_error(index::IndexError error)
{
    auto message = error.message();
    return make_error(ExecutionErrorCode::IndexError, std::move(message), std::move(error));
}

[[nodiscard]]
ExecutionError from_vector_index_error(vindex::VectorIndexError error)
{
    auto message = error.message();
    return make_error(ExecutionErrorCode::IndexError, std::move(message), std::move(error));
}

[[nodiscard]]
ExecutionError from_transaction_error(transaction::TransactionError error)
{
    const auto * context = error.context<transaction::TransactionErrorContext>();
    auto message =
        "Transaction " +
        std::to_string(context != nullptr
                           ? context->transaction_id
                           : transaction::InvalidTransactionId) +
        ": " + error.message();
    return make_error(
        ExecutionErrorCode::TransactionError,
        std::move(message),
        std::move(error)
    );
}

[[nodiscard]]
ExecutionError from_evaluation_error(
    evaluator::EvaluationError error
)
{
    auto message = error.message();
    return make_error(
        ExecutionErrorCode::EvaluationError,
        std::move(message),
        std::move(error)
    );
}

[[nodiscard]]
ExecutionResult command_result(std::size_t affected_rows)
{
    ExecutionResult result;
    result.kind = ExecutionResultKind::Command;
    result.affected_rows = affected_rows;
    return result;
}

[[nodiscard]]
ExecutionResult rowset_result(std::vector<ExecutionColumn> columns, std::vector<ExecutionRow> rows)
{
    ExecutionResult result;
    result.kind = ExecutionResultKind::RowSet;
    result.columns = std::move(columns);
    result.rows = std::move(rows);
    return result;
}

[[nodiscard]]
std::string logical_type_name(const LogicalType & value)
{
    std::string name;
    switch (value.id) {
    case LogicalTypeId::Null:
        name = "NULL";
        break;
    case LogicalTypeId::Boolean:
        name = "BOOLEAN";
        break;
    case LogicalTypeId::Integer:
        name = "INTEGER";
        break;
    case LogicalTypeId::BigInt:
        name = "BIGINT";
        break;
    case LogicalTypeId::Float:
        name = "FLOAT";
        break;
    case LogicalTypeId::Double:
        name = "DOUBLE";
        break;
    case LogicalTypeId::Varchar:
        name = "VARCHAR";
        break;
    case LogicalTypeId::Vector:
        name = "VECTOR";
        break;
    }

    if (value.parameter.has_value()) {
        name.append("(").append(std::to_string(value.parameter.value())).append(")");
    }
    return name;
}

[[nodiscard]]
std::string index_kind_name(meta::entry::IndexKind kind)
{
    switch (kind) {
    case meta::entry::IndexKind::BTree:
        return "BTREE";
    }
    return "UNKNOWN";
}

[[nodiscard]]
std::string vector_index_kind_name(meta::entry::VectorIndexKind kind)
{
    switch (kind) {
    case meta::entry::VectorIndexKind::Hnsw:
        return "HNSW";
    }
    return "UNKNOWN";
}

[[nodiscard]]
std::string vector_metric_name(meta::entry::VectorDistanceMetric metric)
{
    switch (metric) {
    case meta::entry::VectorDistanceMetric::L2:
        return "L2";
    case meta::entry::VectorDistanceMetric::InnerProduct:
        return "INNER_PRODUCT";
    case meta::entry::VectorDistanceMetric::Cosine:
        return "COSINE";
    }
    return "UNKNOWN";
}

[[nodiscard]]
std::expected<void, ExecutionError> find_storage(
    storage::StorageEngine & storage,
    common::CollectionId collection_id
)
{
    if (!storage.contains_collection(collection_id)) {
        return std::unexpected(make_error(
            ExecutionErrorCode::CollectionNotFound,
                        "Collection storage not found"
        ));
    }
    return {};
}

[[nodiscard]]
std::expected<schema::CollectionSchema, ExecutionError> load_schema(
    meta::CatalogView & catalog,
    common::CollectionId collection_id
)
{
    auto collection_schema = storage::load_collection_schema(catalog, collection_id);
    if (!collection_schema.has_value()) {
        return std::unexpected(from_schema_error(std::move(collection_schema.error())));
    }
    return std::move(*collection_schema);
}

[[nodiscard]]
std::expected<PipelineResult, ExecutionError> execute_physical(
    const PhysicalOperator & node,
    meta::CatalogView & catalog,
    storage::StorageEngine & storage,
    index::IndexEngine & index_engine,
    vindex::VectorIndexEngine & vector_index_engine
);

void append_pipeline_row(
    PipelineResult & result,
    common::Record record
)
{
    result.rows.push_back(PipelineRow {
        .source_record = std::move(record),
        .output_values = {},
    });
    result.rows.back().output_values = result.rows.back().source_record.data.values;
}

void append_scan_columns(PipelineResult & result, const schema::CollectionSchema & collection_schema)
{
    for (const auto & column : collection_schema.columns()) {
        result.columns.push_back(ExecutionColumn {
            .name = column.column_name(),
            .type = column.type(),
        });
    }
}

[[nodiscard]]
std::expected<PipelineResult, ExecutionError> execute_scan(
    const SeqScanOperator & scan,
    meta::CatalogView & catalog,
    storage::StorageEngine & storage
)
{
    auto collection_schema = load_schema(catalog, scan.collection_id());
    if (!collection_schema.has_value()) {
        return std::unexpected(std::move(collection_schema.error()));
    }

    auto collection_storage = find_storage(storage, scan.collection_id());
    if (!collection_storage.has_value()) {
        return std::unexpected(std::move(collection_storage.error()));
    }

    PipelineResult result;
    append_scan_columns(result, *collection_schema);

    auto cursor = storage.scan(scan.collection_id());
    if (!cursor) return std::unexpected(from_storage_error(std::move(cursor.error())));
    while (true) {
        auto next = cursor->next();
        if (!next) return std::unexpected(from_storage_error(std::move(next.error())));
        if (!*next) break;
        append_pipeline_row(result, std::move(**next));
    }

    return result;
}

[[nodiscard]]
std::expected<index::IndexRange, ExecutionError> index_range_from_lookup(
    const IndexScanOperator & scan
)
{
    const auto & lookup = scan.lookup();
    if (lookup.kind == IndexLookupKind::Equal) {
        if (!lookup.lower.has_value()) {
            return std::unexpected(make_error(
                ExecutionErrorCode::InvalidPlan,
                "Physical index equality lookup is missing its key"
            ));
        }
        return index::IndexRange::closed(lookup.lower->key, lookup.lower->key);
    }

    if (lookup.lower.has_value() && lookup.upper.has_value()) {
        return index::IndexRange::between(
            lookup.lower->key,
            lookup.lower->inclusive,
            lookup.upper->key,
            lookup.upper->inclusive
        );
    }
    if (lookup.lower.has_value()) {
        return index::IndexRange::lower_bound(lookup.lower->key, lookup.lower->inclusive);
    }
    if (lookup.upper.has_value()) {
        return index::IndexRange::upper_bound(lookup.upper->key, lookup.upper->inclusive);
    }
    return index::IndexRange::all();
}

[[nodiscard]]
std::expected<PipelineResult, ExecutionError> execute_index_scan(
    const IndexScanOperator & scan,
    meta::CatalogView & catalog,
    storage::StorageEngine & storage,
    index::IndexEngine & index_engine
)
{
    auto index_view = index_engine.find_index(scan.index_id());
    if (!index_view.has_value()) {
        return std::unexpected(make_error(
            ExecutionErrorCode::IndexError,
            "Physical index scan target index was not found"
        ));
    }
    if (index_view->collection_id != scan.collection_id() || index_view->kind != index::IndexKind::BTree) {
        return std::unexpected(make_error(
            ExecutionErrorCode::InvalidPlan,
            "Physical index scan does not match the runtime index descriptor"
        ));
    }

    auto collection_schema = load_schema(catalog, scan.collection_id());
    if (!collection_schema.has_value()) {
        return std::unexpected(std::move(collection_schema.error()));
    }

    auto collection_storage = find_storage(storage, scan.collection_id());
    if (!collection_storage.has_value()) {
        return std::unexpected(std::move(collection_storage.error()));
    }

    std::expected<std::vector<common::RecordId>, index::IndexError> record_ids;
    if (scan.lookup().kind == IndexLookupKind::Equal) {
        if (!scan.lookup().lower.has_value()) {
            return std::unexpected(make_error(
                ExecutionErrorCode::InvalidPlan,
                "Physical index equality lookup is missing its key"
            ));
        }
        record_ids = index_engine.find_equal(scan.index_id(), scan.lookup().lower->key);
    } else {
        auto range = index_range_from_lookup(scan);
        if (!range.has_value()) {
            return std::unexpected(std::move(range.error()));
        }
        auto cursor = index_engine.scan_range_cursor(scan.index_id(), *range);
        if (!cursor.has_value()) {
            return std::unexpected(from_index_error(std::move(cursor.error())));
        }
        PipelineResult result;
        append_scan_columns(result, *collection_schema);
        while (true) {
            auto next = (*cursor)->next();
            if (!next.has_value()) {
                return std::unexpected(from_index_error(std::move(next.error())));
            }
            if (!*next) {
                break;
            }
            auto record = storage.get(scan.collection_id(), **next);
            if (!record.has_value()) {
                return std::unexpected(from_storage_error(std::move(record.error())));
            }
            append_pipeline_row(result, std::move(*record));
        }
        return result;
    }
    if (!record_ids.has_value()) {
        return std::unexpected(from_index_error(std::move(record_ids.error())));
    }

    PipelineResult result;
    append_scan_columns(result, *collection_schema);
    for (const auto record_id : *record_ids) {
        auto record = storage.get(scan.collection_id(), record_id);
        if (!record.has_value()) {
            return std::unexpected(from_storage_error(std::move(record.error())));
        }
        append_pipeline_row(result, std::move(*record));
    }

    return result;
}

[[nodiscard]]
std::expected<void, ExecutionError> apply_predicate(
    PipelineResult & input,
    std::optional<std::reference_wrapper<const BoundExpression>> predicate
)
{
    if (!predicate.has_value()) {
        return {};
    }

    std::vector<PipelineRow> rows;
    rows.reserve(input.rows.size());
    for (auto & row : input.rows) {
        evaluator::ExpressionEvaluator evaluator {evaluator::EvaluationContext {
            .input_values = row.source_record.data.values,
        }};
        auto matched = evaluator.evaluate_filter(predicate->get());
        if (!matched.has_value()) {
            return std::unexpected(from_evaluation_error(std::move(matched.error())));
        }
        if (*matched) {
            rows.push_back(std::move(row));
        }
    }
    input.rows = std::move(rows);
    return {};
}

[[nodiscard]]
std::expected<PipelineResult, ExecutionError> execute_vector_fallback_scan(
    const VectorSearchOperator & search,
    meta::CatalogView & catalog,
    storage::StorageEngine & storage
)
{
    auto collection_schema = load_schema(catalog, search.collection_id());
    if (!collection_schema.has_value()) {
        return std::unexpected(std::move(collection_schema.error()));
    }
    auto collection_storage = find_storage(storage, search.collection_id());
    if (!collection_storage.has_value()) {
        return std::unexpected(std::move(collection_storage.error()));
    }

    PipelineResult result;
    append_scan_columns(result, *collection_schema);
    auto cursor = storage.scan(search.collection_id());
    if (!cursor.has_value()) {
        return std::unexpected(from_storage_error(std::move(cursor.error())));
    }
    while (true) {
        auto next = cursor->next();
        if (!next.has_value()) {
            return std::unexpected(from_storage_error(std::move(next.error())));
        }
        if (!next->has_value()) {
            break;
        }
        append_pipeline_row(result, std::move(**next));
    }
    auto filtered = apply_predicate(result, search.predicate());
    if (!filtered.has_value()) {
        return std::unexpected(std::move(filtered.error()));
    }
    return result;
}

[[nodiscard]]
vindex::VectorDistanceMetric runtime_metric(meta::entry::VectorDistanceMetric metric)
{
    switch (metric) {
    case meta::entry::VectorDistanceMetric::L2:
        return vindex::VectorDistanceMetric::L2;
    case meta::entry::VectorDistanceMetric::InnerProduct:
        return vindex::VectorDistanceMetric::InnerProduct;
    case meta::entry::VectorDistanceMetric::Cosine:
        return vindex::VectorDistanceMetric::Cosine;
    }
    return vindex::VectorDistanceMetric::L2;
}

[[nodiscard]]
std::size_t saturating_multiply(std::size_t value, std::size_t factor)
{
    if (value > std::numeric_limits<std::size_t>::max() / factor) {
        return std::numeric_limits<std::size_t>::max();
    }
    return value * factor;
}

[[nodiscard]]
std::expected<PipelineResult, ExecutionError> execute_vector_search(
    const VectorSearchOperator & search,
    meta::CatalogView & catalog,
    storage::StorageEngine & storage,
    vindex::VectorIndexEngine & vector_index_engine
)
{
    auto collection_schema = load_schema(catalog, search.collection_id());
    if (!collection_schema.has_value()) {
        return std::unexpected(std::move(collection_schema.error()));
    }
    auto collection_storage = find_storage(storage, search.collection_id());
    if (!collection_storage.has_value()) {
        return std::unexpected(std::move(collection_storage.error()));
    }

    auto query_value = evaluator::ExpressionEvaluator::evaluate_constant(search.query_vector());
    if (!query_value.has_value()) {
        return std::unexpected(from_evaluation_error(std::move(query_value.error())));
    }
    auto query_key = vindex::VectorIndexKey::from_value(*query_value);
    if (!query_key.has_value()) {
        return std::unexpected(from_vector_index_error(std::move(query_key.error())));
    }

    const auto view = vector_index_engine.find_index(search.index_id());
    if (!view.has_value()) {
        return std::unexpected(make_error(
            ExecutionErrorCode::IndexError,
            "Physical vector search target index was not found at runtime"
        ));
    }
    if (view->collection_id != search.collection_id() || view->column_id != search.column_id() ||
        view->kind != vindex::VectorIndexKind::Hnsw || view->metric != runtime_metric(search.metric()) ||
        view->dimension != query_key->dimension()) {
        return std::unexpected(make_error(
            ExecutionErrorCode::InvalidPlan,
            "Physical vector search does not match the runtime vector index descriptor"
        ));
    }

    const auto full_scan = [&]() {
        return execute_vector_fallback_scan(search, catalog, storage);
    };
    if (search.required_count() == 0 || view->entry_count < search.required_count()) {
        return full_scan();
    }

    auto candidate_count = !search.predicate().has_value()
        ? search.required_count()
        : std::min(view->entry_count, std::max(search.required_count(), saturating_multiply(search.required_count(), 4)));

    while (true) {
        auto matches = vector_index_engine.search(
            search.index_id(), *query_key, vindex::VectorSearchRequest {.top_k = candidate_count}
        );
        if (!matches.has_value()) {
            return std::unexpected(from_vector_index_error(std::move(matches.error())));
        }

        PipelineResult result;
        append_scan_columns(result, *collection_schema);
        result.rows.reserve(matches->size());
        for (const auto & match : *matches) {
            auto record = storage.get(search.collection_id(), match.record_id);
            if (!record.has_value()) {
                return std::unexpected(from_storage_error(std::move(record.error())));
            }
            append_pipeline_row(result, std::move(*record));
        }
        auto filtered = apply_predicate(result, search.predicate());
        if (!filtered.has_value()) {
            return std::unexpected(std::move(filtered.error()));
        }
        if (result.rows.size() >= search.required_count()) {
            return result;
        }
        if (!search.predicate().has_value() || candidate_count >= view->entry_count) {
            return full_scan();
        }
        candidate_count = std::min(view->entry_count, saturating_multiply(candidate_count, 2));
    }
}

[[nodiscard]]
std::expected<PipelineResult, ExecutionError> execute_filter(
    const FilterOperator & filter,
    meta::CatalogView & catalog,
    storage::StorageEngine & storage,
    index::IndexEngine & index_engine,
    vindex::VectorIndexEngine & vector_index_engine
)
{
    auto input = execute_physical(filter.child(), catalog, storage, index_engine, vector_index_engine);
    if (!input.has_value()) {
        return std::unexpected(std::move(input.error()));
    }

    std::vector<PipelineRow> rows;
    for (auto & row : input->rows) {
        evaluator::ExpressionEvaluator evaluator {evaluator::EvaluationContext {
            .input_values = row.source_record.data.values,
        }};
        auto predicate = evaluator.evaluate_filter(filter.predicate());
        if (!predicate.has_value()) {
            return std::unexpected(from_evaluation_error(std::move(predicate.error())));
        }

        if (*predicate) {
            rows.push_back(std::move(row));
        }
    }

    input->rows = std::move(rows);
    return input;
}

[[nodiscard]]
std::string projection_name(const binder::bound::BoundProjectionItem & projection, std::size_t index)
{
    return projection.output_name.empty()
        ? "expr" + std::to_string(index + 1)
        : projection.output_name;
}

[[nodiscard]]
std::expected<PipelineResult, ExecutionError> execute_projection(
    const ProjectionOperator & projection,
    meta::CatalogView & catalog,
    storage::StorageEngine & storage,
    index::IndexEngine & index_engine,
    vindex::VectorIndexEngine & vector_index_engine
)
{
    auto input = execute_physical(projection.child(), catalog, storage, index_engine, vector_index_engine);
    if (!input.has_value()) {
        return std::unexpected(std::move(input.error()));
    }

    input->columns.clear();
    const auto & projections = projection.projections();
    input->columns.reserve(projections.size());
    for (std::size_t index = 0; index < projections.size(); ++index) {
        if (projections[index].expression == nullptr) {
            return std::unexpected(make_error(
                ExecutionErrorCode::InvalidPlan,
                "Physical projection expression is missing"
            ));
        }
        input->columns.push_back(ExecutionColumn {
            .name = projection_name(projections[index], index),
            .type = projections[index].expression->type(),
        });
    }

    for (auto & row : input->rows) {
        evaluator::ExpressionEvaluator evaluator {evaluator::EvaluationContext {
            .input_values = row.source_record.data.values,
        }};
        std::vector<common::Value> values;
        values.reserve(projections.size());
        for (const auto & projection : projections) {
            auto value = evaluator.evaluate(*projection.expression);
            if (!value.has_value()) {
                return std::unexpected(from_evaluation_error(std::move(value.error())));
            }
            values.push_back(std::move(*value));
        }
        row.output_values = std::move(values);
    }

    return input;
}

[[nodiscard]]
int value_rank(const common::Value & value)
{
    return static_cast<int>(value.data().index());
}

[[nodiscard]]
int compare_values(const common::Value & left, const common::Value & right)
{
    if (left.is_null() && right.is_null()) {
        return 0;
    }
    if (left.is_null()) {
        return 1;
    }
    if (right.is_null()) {
        return -1;
    }

    return std::visit(
        [](const auto & left_data, const auto & right_data) -> int {
            using Left = std::decay_t<decltype(left_data)>;
            using Right = std::decay_t<decltype(right_data)>;
            if constexpr (std::is_same_v<Left, Right> && requires { left_data < right_data; }) {
                if (left_data < right_data) {
                    return -1;
                }
                if (right_data < left_data) {
                    return 1;
                }
                return 0;
            } else {
                return 0;
            }
        },
        left.data(),
        right.data()
    );
}

[[nodiscard]]
std::expected<std::vector<common::Value>, ExecutionError> evaluate_order_keys(
    const SortOperator & order_by,
    const PipelineRow & row
)
{
    evaluator::ExpressionEvaluator evaluator {evaluator::EvaluationContext {
        .input_values = row.source_record.data.values,
    }};
    std::vector<common::Value> keys;
    keys.reserve(order_by.order_by().size());
    for (const auto & item : order_by.order_by()) {
        if (item.expression == nullptr) {
            return std::unexpected(make_error(
                ExecutionErrorCode::InvalidPlan,
                "Physical sort expression is missing"
            ));
        }
        auto value = evaluator.evaluate(*item.expression);
        if (!value.has_value()) {
            return std::unexpected(from_evaluation_error(std::move(value.error())));
        }
        keys.push_back(std::move(*value));
    }
    return keys;
}

[[nodiscard]]
std::expected<PipelineResult, ExecutionError> execute_order_by(
    const SortOperator & order_by,
    meta::CatalogView & catalog,
    storage::StorageEngine & storage,
    index::IndexEngine & index_engine,
    vindex::VectorIndexEngine & vector_index_engine
)
{
    auto input = execute_physical(order_by.child(), catalog, storage, index_engine, vector_index_engine);
    if (!input.has_value()) {
        return std::unexpected(std::move(input.error()));
    }

    struct SortRow
    {
        PipelineRow row;
        std::vector<common::Value> keys;
        std::size_t position {0};
    };

    std::vector<SortRow> sort_rows;
    sort_rows.reserve(input->rows.size());
    for (std::size_t position = 0; position < input->rows.size(); ++position) {
        auto keys = evaluate_order_keys(order_by, input->rows[position]);
        if (!keys.has_value()) {
            return std::unexpected(std::move(keys.error()));
        }
        sort_rows.push_back(SortRow {
            .row = std::move(input->rows[position]),
            .keys = std::move(*keys),
            .position = position,
        });
    }

    std::stable_sort(
        sort_rows.begin(),
        sort_rows.end(),
        [&order_by](const SortRow & left, const SortRow & right) {
            for (std::size_t index = 0; index < order_by.order_by().size(); ++index) {
                if (left.keys[index].is_null() && right.keys[index].is_null()) {
                    continue;
                }
                if (left.keys[index].is_null()) {
                    return false;
                }
                if (right.keys[index].is_null()) {
                    return true;
                }

                auto compared = compare_values(left.keys[index], right.keys[index]);
                if (compared == 0 && value_rank(left.keys[index]) != value_rank(right.keys[index])) {
                    compared = value_rank(left.keys[index]) < value_rank(right.keys[index]) ? -1 : 1;
                }
                if (compared == 0) {
                    continue;
                }
                return order_by.order_by()[index].ascending ? compared < 0 : compared > 0;
            }
            return left.position < right.position;
        }
    );

    input->rows.clear();
    input->rows.reserve(sort_rows.size());
    for (auto & row : sort_rows) {
        input->rows.push_back(std::move(row.row));
    }

    return input;
}

[[nodiscard]]
std::expected<PipelineResult, ExecutionError> execute_limit(
    const LimitOperator & limit,
    meta::CatalogView & catalog,
    storage::StorageEngine & storage,
    index::IndexEngine & index_engine,
    vindex::VectorIndexEngine & vector_index_engine
)
{
    auto input = execute_physical(limit.child(), catalog, storage, index_engine, vector_index_engine);
    if (!input.has_value()) {
        return std::unexpected(std::move(input.error()));
    }

    const auto offset = limit.offset().value_or(0);
    if (offset >= input->rows.size()) {
        input->rows.clear();
        return input;
    }

    const auto count = limit.limit().value_or(input->rows.size() - offset);
    const auto begin = input->rows.begin() + static_cast<std::ptrdiff_t>(offset);
    const auto end = begin + static_cast<std::ptrdiff_t>(std::min(count, input->rows.size() - offset));
    std::vector<PipelineRow> rows;
    rows.reserve(static_cast<std::size_t>(end - begin));
    for (auto it = begin; it != end; ++it) {
        rows.push_back(std::move(*it));
    }

    input->rows = std::move(rows);
    return input;
}

std::expected<PipelineResult, ExecutionError> execute_physical(
    const PhysicalOperator & node,
    meta::CatalogView & catalog,
    storage::StorageEngine & storage,
    index::IndexEngine & index_engine,
    vindex::VectorIndexEngine & vector_index_engine
)
{
    switch (node.kind()) {
    case PhysicalOperatorKind::SeqScan:
        return execute_scan(static_cast<const SeqScanOperator &>(node), catalog, storage);
    case PhysicalOperatorKind::IndexScan:
        return execute_index_scan(static_cast<const IndexScanOperator &>(node), catalog, storage, index_engine);
    case PhysicalOperatorKind::VectorSearch:
        return execute_vector_search(
            static_cast<const VectorSearchOperator &>(node), catalog, storage, vector_index_engine
        );
    case PhysicalOperatorKind::Filter:
        return execute_filter(static_cast<const FilterOperator &>(node), catalog, storage, index_engine, vector_index_engine);
    case PhysicalOperatorKind::Projection:
        return execute_projection(static_cast<const ProjectionOperator &>(node), catalog, storage, index_engine, vector_index_engine);
    case PhysicalOperatorKind::Sort:
        return execute_order_by(static_cast<const SortOperator &>(node), catalog, storage, index_engine, vector_index_engine);
    case PhysicalOperatorKind::Limit:
        return execute_limit(static_cast<const LimitOperator &>(node), catalog, storage, index_engine, vector_index_engine);
    }

    return std::unexpected(make_error(ExecutionErrorCode::InvalidPlan, "Unknown physical plan node"));
}

[[nodiscard]]
std::expected<ExecutionResult, ExecutionError> execute_query(
    const QueryPlan & plan,
    meta::CatalogView & catalog,
    storage::StorageEngine & storage,
    index::IndexEngine & index_engine,
    vindex::VectorIndexEngine & vector_index_engine
)
{
    auto pipeline = execute_physical(plan.root_operator(), catalog, storage, index_engine, vector_index_engine);
    if (!pipeline.has_value()) {
        return std::unexpected(std::move(pipeline.error()));
    }

    std::vector<ExecutionRow> rows;
    rows.reserve(pipeline->rows.size());
    for (auto & row : pipeline->rows) {
        rows.push_back(ExecutionRow {.values = std::move(row.output_values)});
    }

    return rowset_result(std::move(pipeline->columns), std::move(rows));
}

[[nodiscard]]
std::expected<ExecutionResult, ExecutionError> execute_use(
    const UsePlan & plan,
    meta::CatalogView & catalog
)
{
    const auto * database = catalog.find_database(plan.database_id());
    if (database == nullptr) {
        return std::unexpected(make_error(
            ExecutionErrorCode::InvalidPlan,
            "USE target database was not found"
        ));
    }
    ExecutionResult result;
    result.kind = ExecutionResultKind::UseDatabase;
    result.selected_database_id = plan.database_id();
    result.selected_database_name = database->name();
    return result;
}

[[nodiscard]]
std::expected<ExecutionResult, ExecutionError> execute_insert(
    const InsertPlan & plan,
    storage::StorageEngine & storage,
    transaction::TransactionManager & transaction_manager
)
{
    auto collection_storage = find_storage(storage, plan.collection_id());
    if (!collection_storage.has_value()) {
        return std::unexpected(std::move(collection_storage.error()));
    }

    common::RecordData record_data;
    record_data.values.reserve(plan.values().size());
    for (const auto & expression : plan.values()) {
        if (expression == nullptr) {
            return std::unexpected(make_error(
                ExecutionErrorCode::InvalidPlan,
                "Physical INSERT value expression is missing"
            ));
        }
        auto value = evaluator::ExpressionEvaluator::evaluate_constant(*expression);
        if (!value.has_value()) {
            return std::unexpected(from_evaluation_error(std::move(value.error())));
        }
        record_data.values.push_back(std::move(*value));
    }

    auto transaction = transaction_manager.begin_implicit();
    if (!transaction) return std::unexpected(from_transaction_error(std::move(transaction.error())));
    auto staged = transaction_manager.stage_insert(*transaction, plan.collection_id(), std::move(record_data));
    if (!staged) {
        (void) transaction_manager.abort(*transaction);
        return std::unexpected(from_transaction_error(std::move(staged.error())));
    }
    auto committed = transaction_manager.commit(*transaction);
    if (!committed) return std::unexpected(from_transaction_error(std::move(committed.error())));

    return command_result(1);
}

[[nodiscard]]
std::expected<ExecutionResult, ExecutionError> execute_delete(
    const DeletePlan & plan,
    meta::CatalogView & catalog,
    storage::StorageEngine & storage,
    index::IndexEngine & index_engine,
    vindex::VectorIndexEngine & vector_index_engine,
    transaction::TransactionManager & transaction_manager
)
{
    auto rows = execute_physical(plan.root_operator(), catalog, storage, index_engine, vector_index_engine);
    if (!rows.has_value()) {
        return std::unexpected(std::move(rows.error()));
    }

    auto collection_storage = find_storage(storage, plan.collection_id());
    if (!collection_storage.has_value()) {
        return std::unexpected(std::move(collection_storage.error()));
    }

    auto transaction = transaction_manager.begin_implicit();
    if (!transaction) return std::unexpected(from_transaction_error(std::move(transaction.error())));
    for (const auto & row : rows->rows) {
        auto staged = transaction_manager.stage_delete(
            *transaction, plan.collection_id(), row.source_record.record_id, row.source_record.data
        );
        if (!staged) {
            (void) transaction_manager.abort(*transaction);
            return std::unexpected(from_transaction_error(std::move(staged.error())));
        }
    }
    auto committed = transaction_manager.commit(*transaction);
    if (!committed) return std::unexpected(from_transaction_error(std::move(committed.error())));
    return command_result(rows->rows.size());
}

[[nodiscard]]
std::optional<std::size_t> ordinal_for_column(
    const schema::CollectionSchema & collection_schema,
    common::ColumnId column_id
)
{
    const auto * column = collection_schema.find_column(column_id);
    if (column == nullptr) {
        return std::nullopt;
    }
    return column->ordinal();
}

[[nodiscard]]
std::expected<ExecutionResult, ExecutionError> execute_update(
    const UpdatePlan & plan,
    meta::CatalogView & catalog,
    storage::StorageEngine & storage,
    index::IndexEngine & index_engine,
    vindex::VectorIndexEngine & vector_index_engine,
    transaction::TransactionManager & transaction_manager
)
{
    auto collection_schema = load_schema(catalog, plan.collection_id());
    if (!collection_schema.has_value()) {
        return std::unexpected(std::move(collection_schema.error()));
    }

    auto rows = execute_physical(plan.root_operator(), catalog, storage, index_engine, vector_index_engine);
    if (!rows.has_value()) {
        return std::unexpected(std::move(rows.error()));
    }

    auto collection_storage = find_storage(storage, plan.collection_id());
    if (!collection_storage.has_value()) {
        return std::unexpected(std::move(collection_storage.error()));
    }

    auto transaction = transaction_manager.begin_implicit();
    if (!transaction) return std::unexpected(from_transaction_error(std::move(transaction.error())));
    for (const auto & row : rows->rows) {
        auto record_data = row.source_record.data;

        for (const auto & assignment : plan.assignments()) {
            if (assignment.value == nullptr) {
                (void) transaction_manager.abort(*transaction);
                return std::unexpected(make_error(
                    ExecutionErrorCode::InvalidPlan,
                    "Physical UPDATE assignment expression is missing"
                ));
            }
            auto ordinal = ordinal_for_column(*collection_schema, assignment.column_id);
            if (!ordinal.has_value() || *ordinal >= record_data.values.size()) {
                (void) transaction_manager.abort(*transaction);
                return std::unexpected(make_error(
                    ExecutionErrorCode::InvalidPlan,
                    "Assignment column is not in collection schema"
                ));
            }

            evaluator::ExpressionEvaluator evaluator {evaluator::EvaluationContext {
                .input_values = row.source_record.data.values,
            }};
            auto value = evaluator.evaluate(*assignment.value);
            if (!value.has_value()) {
                (void) transaction_manager.abort(*transaction);
                return std::unexpected(from_evaluation_error(std::move(value.error())));
            }
            record_data.values[*ordinal] = std::move(*value);
        }

        auto staged = transaction_manager.stage_update(
            *transaction,
            plan.collection_id(),
            row.source_record.record_id,
            row.source_record.data,
            std::move(record_data)
        );
        if (!staged) {
            (void) transaction_manager.abort(*transaction);
            return std::unexpected(from_transaction_error(std::move(staged.error())));
        }
    }
    auto committed = transaction_manager.commit(*transaction);
    if (!committed) return std::unexpected(from_transaction_error(std::move(committed.error())));
    return command_result(rows->rows.size());
}

[[nodiscard]]
std::expected<ExecutionResult, ExecutionError> execute_show_databases(meta::CatalogView & catalog)
{
    std::vector<ExecutionRow> rows;
    for (const auto * database : catalog.list_databases()) {
        if (database != nullptr) {
            rows.push_back(ExecutionRow {.values = {common::Value {database->name()}}});
        }
    }

    return rowset_result(
        {ExecutionColumn {.name = "database_name", .type = type(LogicalTypeId::Varchar)}},
        std::move(rows)
    );
}

[[nodiscard]]
std::expected<ExecutionResult, ExecutionError> execute_show_collections(
    const ShowCollectionsPlan & plan,
    meta::CatalogView & catalog
)
{
    std::vector<ExecutionRow> rows;
    for (const auto * collection : catalog.list_collections(plan.database_id())) {
        if (collection != nullptr) {
            rows.push_back(ExecutionRow {.values = {common::Value {collection->name()}}});
        }
    }

    return rowset_result(
        {ExecutionColumn {.name = "collection_name", .type = type(LogicalTypeId::Varchar)}},
        std::move(rows)
    );
}

[[nodiscard]]
std::expected<ExecutionResult, ExecutionError> execute_show_indexes(
    const ShowIndexesPlan & plan,
    meta::CatalogView & catalog
)
{
    std::vector<ExecutionRow> rows;
    for (const auto * index : catalog.list_indexes(plan.collection_id())) {
        if (index == nullptr) {
            continue;
        }

        const auto column_id = index->column_id();
        const auto * column = column_id.has_value() ? catalog.find_column(*column_id) : nullptr;
        rows.push_back(ExecutionRow {
            .values = {
                common::Value {index->name()},
                column != nullptr ? common::Value {column->name()} : common::Value::null(),
                common::Value {index_kind_name(index->kind())},
                common::Value {index->unique()},
            },
        });
    }

    return rowset_result(
        {
            ExecutionColumn {.name = "index_name", .type = type(LogicalTypeId::Varchar)},
            ExecutionColumn {.name = "column_name", .type = type(LogicalTypeId::Varchar)},
            ExecutionColumn {.name = "type", .type = type(LogicalTypeId::Varchar)},
            ExecutionColumn {.name = "unique", .type = type(LogicalTypeId::Boolean)},
        },
        std::move(rows)
    );
}

[[nodiscard]]
std::expected<ExecutionResult, ExecutionError> execute_show_vector_indexes(
    const ShowVectorIndexesPlan & plan,
    meta::CatalogView & catalog
)
{
    std::vector<ExecutionRow> rows;
    for (const auto * index : catalog.list_vector_indexes(plan.collection_id())) {
        if (index == nullptr) {
            continue;
        }

        const auto * column = catalog.find_column(index->column_id());
        rows.push_back(ExecutionRow {
            .values = {
                common::Value {index->name()},
                column != nullptr ? common::Value {column->name()} : common::Value::null(),
                common::Value {vector_index_kind_name(index->index_kind())},
                common::Value {vector_metric_name(index->metric())},
                common::Value {static_cast<std::int64_t>(index->dimension())},
                common::Value {static_cast<std::int64_t>(index->max_neighbors())},
                common::Value {static_cast<std::int64_t>(index->ef_construction())},
                common::Value {static_cast<std::int64_t>(index->ef_search_default())},
                common::Value {static_cast<std::int64_t>(index->random_seed())},
            },
        });
    }

    return rowset_result(
        {
            ExecutionColumn {.name = "index_name", .type = type(LogicalTypeId::Varchar)},
            ExecutionColumn {.name = "column_name", .type = type(LogicalTypeId::Varchar)},
            ExecutionColumn {.name = "type", .type = type(LogicalTypeId::Varchar)},
            ExecutionColumn {.name = "metric", .type = type(LogicalTypeId::Varchar)},
            ExecutionColumn {.name = "dimension", .type = type(LogicalTypeId::BigInt)},
            ExecutionColumn {.name = "max_neighbors", .type = type(LogicalTypeId::BigInt)},
            ExecutionColumn {.name = "ef_construction", .type = type(LogicalTypeId::BigInt)},
            ExecutionColumn {.name = "ef_search", .type = type(LogicalTypeId::BigInt)},
            ExecutionColumn {.name = "random_seed", .type = type(LogicalTypeId::BigInt)},
        },
        std::move(rows)
    );
}

[[nodiscard]]
std::expected<ExecutionResult, ExecutionError> execute_describe_collection(
    const DescribeCollectionPlan & plan,
    meta::CatalogView & catalog
)
{
    auto collection_schema = load_schema(catalog, plan.collection_id());
    if (!collection_schema.has_value()) {
        return std::unexpected(std::move(collection_schema.error()));
    }

    std::vector<ExecutionRow> rows;
    for (const auto & column : collection_schema->columns()) {
        rows.push_back(ExecutionRow {
            .values = {
                common::Value {column.column_name()},
                common::Value {logical_type_name(column.type())},
                common::Value {column.nullable()},
                common::Value {column.unique()},
                column.comment().has_value() ? common::Value {column.comment().value()} : common::Value::null(),
                collection_schema->comment().has_value() ? common::Value {collection_schema->comment().value()} : common::Value::null(),
            },
        });
    }

    return rowset_result(
        {
            ExecutionColumn {.name = "column_name", .type = type(LogicalTypeId::Varchar)},
            ExecutionColumn {.name = "type", .type = type(LogicalTypeId::Varchar)},
            ExecutionColumn {.name = "nullable", .type = type(LogicalTypeId::Boolean)},
            ExecutionColumn {.name = "unique", .type = type(LogicalTypeId::Boolean)},
            ExecutionColumn {.name = "comment", .type = type(LogicalTypeId::Varchar)},
            ExecutionColumn {.name = "collection_comment", .type = type(LogicalTypeId::Varchar)},
        },
        std::move(rows)
    );
}

} // namespace

Executor::Executor(
    meta::CatalogView catalog,
    storage::StorageEngine & storage,
    index::IndexEngine & index_engine,
    vindex::VectorIndexEngine & vector_index_engine
) noexcept
    : catalog_(catalog)
    , storage_(storage)
    , index_engine_(index_engine)
    , vector_index_engine_(&vector_index_engine)
{
}

Executor::Executor(
    meta::CatalogView catalog,
    storage::StorageEngine & storage,
    index::IndexEngine & index_engine,
    vindex::VectorIndexEngine & vector_index_engine,
    transaction::TransactionManager & transaction_manager
) noexcept
    : catalog_(catalog)
    , storage_(storage)
    , index_engine_(index_engine)
    , vector_index_engine_(&vector_index_engine)
    , transaction_manager_(&transaction_manager)
{
}

std::expected<ExecutionResult, ExecutionError> Executor::execute(const PhysicalPlan & plan)
{
    switch (plan.kind()) {
    case PhysicalPlanKind::Use:
        return execute_use(static_cast<const UsePlan &>(plan), catalog_);
    case PhysicalPlanKind::CreateDatabase:
    case PhysicalPlanKind::CreateCollection:
    case PhysicalPlanKind::CreateIndex:
    case PhysicalPlanKind::CreateVectorIndex:
    case PhysicalPlanKind::DropDatabase:
    case PhysicalPlanKind::DropCollection:
    case PhysicalPlanKind::DropIndex:
    case PhysicalPlanKind::DropVectorIndex:
        return std::unexpected(make_error(ExecutionErrorCode::UnsupportedStatement, "DDL must be executed by DatabaseEngine"));
    case PhysicalPlanKind::ShowDatabases:
        return execute_show_databases(catalog_);
    case PhysicalPlanKind::ShowCollections:
        return execute_show_collections(static_cast<const ShowCollectionsPlan &>(plan), catalog_);
    case PhysicalPlanKind::ShowIndexes:
        return execute_show_indexes(static_cast<const ShowIndexesPlan &>(plan), catalog_);
    case PhysicalPlanKind::ShowVectorIndexes:
        return execute_show_vector_indexes(static_cast<const ShowVectorIndexesPlan &>(plan), catalog_);
    case PhysicalPlanKind::DescribeCollection:
        return execute_describe_collection(static_cast<const DescribeCollectionPlan &>(plan), catalog_);
    case PhysicalPlanKind::Insert:
        if (transaction_manager_ == nullptr) {
            return std::unexpected(make_error(ExecutionErrorCode::TransactionError,
                                              "DML execution requires a TransactionManager"));
        }
        return execute_insert(
            static_cast<const InsertPlan &>(plan), storage_, *transaction_manager_
        );
    case PhysicalPlanKind::Update:
        if (transaction_manager_ == nullptr) {
            return std::unexpected(make_error(ExecutionErrorCode::TransactionError,
                                              "DML execution requires a TransactionManager"));
        }
        return execute_update(
            static_cast<const UpdatePlan &>(plan), catalog_, storage_, index_engine_, *vector_index_engine_,
            *transaction_manager_
        );
    case PhysicalPlanKind::Delete:
        if (transaction_manager_ == nullptr) {
            return std::unexpected(make_error(ExecutionErrorCode::TransactionError,
                                              "DML execution requires a TransactionManager"));
        }
        return execute_delete(
            static_cast<const DeletePlan &>(plan), catalog_, storage_, index_engine_, *vector_index_engine_,
            *transaction_manager_
        );
    case PhysicalPlanKind::Query:
        return execute_query(
            static_cast<const QueryPlan &>(plan), catalog_, storage_, index_engine_, *vector_index_engine_
        );
    }

    return std::unexpected(make_error(ExecutionErrorCode::UnsupportedStatement, "Unsupported statement"));
}

} // namespace litedb::core::executor
