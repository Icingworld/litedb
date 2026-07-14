#include "core/executor/executor.hpp"

#include <algorithm>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "core/binder/bound/expression/bound_column_ref_expression.hpp"
#include "core/meta/meta.hpp"
#include "core/evaluator/expression_evaluator.hpp"
#include "core/index/index_manager.hpp"
#include "core/index/scalar_index.hpp"
#include "core/physical_plan/node/physical_filter.hpp"
#include "core/physical_plan/node/physical_index_scan.hpp"
#include "core/physical_plan/node/physical_limit.hpp"
#include "core/physical_plan/node/physical_plan_node.hpp"
#include "core/physical_plan/node/physical_projection.hpp"
#include "core/physical_plan/node/physical_seq_scan.hpp"
#include "core/physical_plan/node/physical_sort.hpp"
#include "core/physical_plan/statement/physical_command_plan.hpp"
#include "core/physical_plan/statement/physical_insert_plan.hpp"
#include "core/physical_plan/statement/physical_query_plan.hpp"
#include "core/physical_plan/statement/physical_row_mutation_plan.hpp"
#include "core/physical_plan/statement/physical_statement_plan.hpp"
#include "core/schema/schema_loader.hpp"

namespace litedb::core::executor
{

namespace
{

using binder::bound::BoundColumnRefExpression;
using binder::bound::BoundExpression;
using common::LogicalType;
using common::LogicalTypeId;
using parser::ast::AstNodeLocation;
using physical_plan::PhysicalCreateCollectionPlan;
using physical_plan::PhysicalCreateDatabasePlan;
using physical_plan::PhysicalCreateIndexPlan;
using physical_plan::PhysicalCreateVectorIndexPlan;
using physical_plan::PhysicalDeletePlan;
using physical_plan::PhysicalDescribeCollectionPlan;
using physical_plan::PhysicalDropCollectionPlan;
using physical_plan::PhysicalDropDatabasePlan;
using physical_plan::PhysicalDropIndexPlan;
using physical_plan::PhysicalDropVectorIndexPlan;
using physical_plan::PhysicalFilter;
using physical_plan::PhysicalIndexLookupKind;
using physical_plan::PhysicalIndexScan;
using physical_plan::PhysicalInsertPlan;
using physical_plan::PhysicalLimit;
using physical_plan::PhysicalPlanNode;
using physical_plan::PhysicalPlanNodeKind;
using physical_plan::PhysicalProjection;
using physical_plan::PhysicalQueryPlan;
using physical_plan::PhysicalSeqScan;
using physical_plan::PhysicalShowCollectionsPlan;
using physical_plan::PhysicalShowIndexesPlan;
using physical_plan::PhysicalShowVectorIndexesPlan;
using physical_plan::PhysicalSort;
using physical_plan::PhysicalStatementPlan;
using physical_plan::PhysicalStatementPlanKind;
using physical_plan::PhysicalUpdatePlan;
using physical_plan::PhysicalUsePlan;

constexpr AstNodeLocation internal_location {0, 0};

struct PipelineRow
{
    schema::Record source_record;
    schema::Record evaluation_record;
    std::vector<schema::Value> output_values;
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
ExecutionError make_error(ExecutionErrorCode code, AstNodeLocation location, std::string message)
{
    return ExecutionError {code, location, std::move(message)};
}

[[nodiscard]]
ExecutionError from_meta_error(meta::MetaEngineError error, AstNodeLocation location)
{
    return make_error(ExecutionErrorCode::MetaError, location, std::move(error.message));
}

[[nodiscard]]
ExecutionError from_schema_error(schema::SchemaError error, AstNodeLocation location)
{
    return make_error(ExecutionErrorCode::SchemaError, location, std::move(error.message));
}

[[nodiscard]]
ExecutionError from_storage_error(storage::StorageError error, AstNodeLocation location)
{
    return make_error(ExecutionErrorCode::StorageError, location, std::move(error.message));
}

[[nodiscard]]
ExecutionError from_index_error(index::IndexError error, AstNodeLocation location)
{
    return make_error(ExecutionErrorCode::IndexError, location, std::move(error.message));
}

[[nodiscard]]
ExecutionError from_evaluation_error(evaluator::EvaluationError error)
{
    return make_error(ExecutionErrorCode::EvaluationError, error.location, std::move(error.message));
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
    case meta::entry::IndexKind::Hash:
        return "HASH";
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
schema::Record make_empty_record()
{
    return schema::Record {.record_id = 0, .data = schema::RecordData {}};
}

[[nodiscard]]
schema::Record make_evaluation_record(
    const schema::CollectionSchema & collection_schema,
    const schema::Record & source_record
)
{
    common::ColumnId max_column_id = 0;
    for (const auto & column : collection_schema.columns()) {
        max_column_id = std::max(max_column_id, column.column_id());
    }

    schema::Record evaluation_record;
    evaluation_record.record_id = source_record.record_id;
    evaluation_record.data.values.resize(static_cast<std::size_t>(max_column_id), schema::Value::null());

    for (std::size_t ordinal = 0; ordinal < collection_schema.columns().size(); ++ordinal) {
        if (ordinal >= source_record.data.values.size()) {
            continue;
        }

        const auto column_id = collection_schema.columns()[ordinal].column_id();
        if (column_id == 0) {
            continue;
        }
        evaluation_record.data.values[static_cast<std::size_t>(column_id - 1)] = source_record.data.values[ordinal];
    }

    return evaluation_record;
}

[[nodiscard]]
std::expected<void, ExecutionError> find_storage(
    storage::StorageEngine & storage,
    common::CollectionId collection_id,
    AstNodeLocation location
)
{
    if (!storage.contains_collection(collection_id)) {
        return std::unexpected(make_error(
            ExecutionErrorCode::CollectionNotFound,
            location,
            "Collection storage not found"
        ));
    }
    return {};
}

[[nodiscard]]
std::expected<schema::CollectionSchema, ExecutionError> load_schema(
    meta::MetaEngine & catalog,
    common::CollectionId collection_id,
    AstNodeLocation location
)
{
    auto collection_schema = schema::load_collection_schema(catalog, collection_id);
    if (!collection_schema.has_value()) {
        return std::unexpected(from_schema_error(std::move(collection_schema.error()), location));
    }
    return std::move(collection_schema.value());
}

[[nodiscard]]
std::expected<PipelineResult, ExecutionError> execute_physical(
    const PhysicalPlanNode & node,
    meta::MetaEngine & catalog,
    storage::StorageEngine & storage,
    index::IndexManager & index_manager
);

void append_pipeline_row(
    PipelineResult & result,
    const schema::CollectionSchema & collection_schema,
    schema::Record record
)
{
    auto evaluation_record = make_evaluation_record(collection_schema, record);
    result.rows.push_back(PipelineRow {
        .source_record = std::move(record),
        .evaluation_record = std::move(evaluation_record),
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
    const PhysicalSeqScan & scan,
    meta::MetaEngine & catalog,
    storage::StorageEngine & storage
)
{
    auto collection_schema = load_schema(catalog, scan.collection_id(), scan.location());
    if (!collection_schema.has_value()) {
        return std::unexpected(std::move(collection_schema.error()));
    }

    auto collection_storage = find_storage(storage, scan.collection_id(), scan.location());
    if (!collection_storage.has_value()) {
        return std::unexpected(std::move(collection_storage.error()));
    }

    PipelineResult result;
    append_scan_columns(result, collection_schema.value());

    auto cursor = storage.scan(scan.collection_id());
    if (!cursor) return std::unexpected(from_storage_error(std::move(cursor.error()), scan.location()));
    while (true) {
        auto next = cursor->next();
        if (!next) return std::unexpected(from_storage_error(std::move(next.error()), scan.location()));
        if (!*next) break;
        append_pipeline_row(result, collection_schema.value(), std::move(**next));
    }

    return result;
}

[[nodiscard]]
std::expected<index::IndexRange, ExecutionError> index_range_from_lookup(
    const PhysicalIndexScan & scan
)
{
    const auto & lookup = scan.lookup();
    if (lookup.kind == PhysicalIndexLookupKind::Equal) {
        if (!lookup.lower.has_value()) {
            return std::unexpected(make_error(
                ExecutionErrorCode::InvalidPlan,
                scan.location(),
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
    const PhysicalIndexScan & scan,
    meta::MetaEngine & catalog,
    storage::StorageEngine & storage,
    index::IndexManager & index_manager
)
{
    auto collection_schema = load_schema(catalog, scan.collection_id(), scan.location());
    if (!collection_schema.has_value()) {
        return std::unexpected(std::move(collection_schema.error()));
    }

    auto collection_storage = find_storage(storage, scan.collection_id(), scan.location());
    if (!collection_storage.has_value()) {
        return std::unexpected(std::move(collection_storage.error()));
    }

    auto index_view = index_manager.find_index(scan.index_id());
    if (!index_view.has_value()) {
        return std::unexpected(make_error(
            ExecutionErrorCode::IndexError,
            scan.location(),
            "Physical index scan target index was not found"
        ));
    }

    std::expected<std::vector<common::RecordId>, index::IndexError> record_ids;
    if (scan.lookup().kind == PhysicalIndexLookupKind::Equal) {
        if (!scan.lookup().lower.has_value()) {
            return std::unexpected(make_error(
                ExecutionErrorCode::InvalidPlan,
                scan.location(),
                "Physical index equality lookup is missing its key"
            ));
        }
        record_ids = index_view->index.find_equal(scan.lookup().lower->key);
    } else {
        auto range = index_range_from_lookup(scan);
        if (!range.has_value()) {
            return std::unexpected(std::move(range.error()));
        }
        record_ids = index_view->index.scan_range(range.value());
    }
    if (!record_ids.has_value()) {
        return std::unexpected(from_index_error(std::move(record_ids.error()), scan.location()));
    }

    PipelineResult result;
    append_scan_columns(result, collection_schema.value());
    for (const auto record_id : record_ids.value()) {
        auto record = storage.get(scan.collection_id(), record_id);
        if (!record.has_value()) {
            return std::unexpected(from_storage_error(std::move(record.error()), scan.location()));
        }
        append_pipeline_row(result, collection_schema.value(), std::move(record.value()));
    }

    return result;
}

[[nodiscard]]
std::expected<PipelineResult, ExecutionError> execute_filter(
    const PhysicalFilter & filter,
    meta::MetaEngine & catalog,
    storage::StorageEngine & storage,
    index::IndexManager & index_manager
)
{
    auto input = execute_physical(filter.child(), catalog, storage, index_manager);
    if (!input.has_value()) {
        return std::unexpected(std::move(input.error()));
    }

    evaluator::ExpressionEvaluator evaluator;
    std::vector<PipelineRow> rows;
    for (auto & row : input->rows) {
        auto predicate = evaluator.evaluate_predicate(filter.predicate(), row.evaluation_record);
        if (!predicate.has_value()) {
            return std::unexpected(from_evaluation_error(std::move(predicate.error())));
        }

        if (predicate.value()) {
            rows.push_back(std::move(row));
        }
    }

    input->rows = std::move(rows);
    return input;
}

[[nodiscard]]
std::string projection_name(const BoundExpression & expression, std::size_t index)
{
    if (expression.kind() == binder::bound::BoundExpressionKind::ColumnRef) {
        const auto & column = static_cast<const BoundColumnRefExpression &>(expression);
        return column.column_name();
    }
    return "expr" + std::to_string(index + 1);
}

[[nodiscard]]
std::string projection_name(const binder::bound::BoundProjectionItem & projection, std::size_t index)
{
    if (projection.alias.has_value()) {
        return projection.alias.value();
    }
    return projection_name(*projection.expression, index);
}

[[nodiscard]]
std::expected<PipelineResult, ExecutionError> execute_projection(
    const PhysicalProjection & projection,
    meta::MetaEngine & catalog,
    storage::StorageEngine & storage,
    index::IndexManager & index_manager
)
{
    auto input = execute_physical(projection.child(), catalog, storage, index_manager);
    if (!input.has_value()) {
        return std::unexpected(std::move(input.error()));
    }

    input->columns.clear();
    const auto & projections = projection.projections();
    input->columns.reserve(projections.size());
    for (std::size_t index = 0; index < projections.size(); ++index) {
        input->columns.push_back(ExecutionColumn {
            .name = projection_name(projections[index], index),
            .type = projections[index].expression->type(),
        });
    }

    evaluator::ExpressionEvaluator evaluator;
    for (auto & row : input->rows) {
        std::vector<schema::Value> values;
        values.reserve(projections.size());
        for (const auto & projection : projections) {
            auto value = evaluator.evaluate(*projection.expression, row.evaluation_record);
            if (!value.has_value()) {
                return std::unexpected(from_evaluation_error(std::move(value.error())));
            }
            values.push_back(std::move(value.value()));
        }
        row.output_values = std::move(values);
    }

    return input;
}

[[nodiscard]]
int value_rank(const schema::Value & value)
{
    return static_cast<int>(value.data().index());
}

[[nodiscard]]
int compare_values(const schema::Value & left, const schema::Value & right)
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
std::expected<std::vector<schema::Value>, ExecutionError> evaluate_order_keys(
    const PhysicalSort & order_by,
    const PipelineRow & row
)
{
    evaluator::ExpressionEvaluator evaluator;
    std::vector<schema::Value> keys;
    keys.reserve(order_by.order_by().size());
    for (const auto & item : order_by.order_by()) {
        auto value = evaluator.evaluate(*item.expression, row.evaluation_record);
        if (!value.has_value()) {
            return std::unexpected(from_evaluation_error(std::move(value.error())));
        }
        keys.push_back(std::move(value.value()));
    }
    return keys;
}

[[nodiscard]]
std::expected<PipelineResult, ExecutionError> execute_order_by(
    const PhysicalSort & order_by,
    meta::MetaEngine & catalog,
    storage::StorageEngine & storage,
    index::IndexManager & index_manager
)
{
    auto input = execute_physical(order_by.child(), catalog, storage, index_manager);
    if (!input.has_value()) {
        return std::unexpected(std::move(input.error()));
    }

    struct SortRow
    {
        PipelineRow row;
        std::vector<schema::Value> keys;
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
            .keys = std::move(keys.value()),
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
    const PhysicalLimit & limit,
    meta::MetaEngine & catalog,
    storage::StorageEngine & storage,
    index::IndexManager & index_manager
)
{
    auto input = execute_physical(limit.child(), catalog, storage, index_manager);
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
    const PhysicalPlanNode & node,
    meta::MetaEngine & catalog,
    storage::StorageEngine & storage,
    index::IndexManager & index_manager
)
{
    switch (node.kind()) {
    case PhysicalPlanNodeKind::SeqScan:
        return execute_scan(static_cast<const PhysicalSeqScan &>(node), catalog, storage);
    case PhysicalPlanNodeKind::IndexScan:
        return execute_index_scan(static_cast<const PhysicalIndexScan &>(node), catalog, storage, index_manager);
    case PhysicalPlanNodeKind::Filter:
        return execute_filter(static_cast<const PhysicalFilter &>(node), catalog, storage, index_manager);
    case PhysicalPlanNodeKind::Projection:
        return execute_projection(static_cast<const PhysicalProjection &>(node), catalog, storage, index_manager);
    case PhysicalPlanNodeKind::Sort:
        return execute_order_by(static_cast<const PhysicalSort &>(node), catalog, storage, index_manager);
    case PhysicalPlanNodeKind::Limit:
        return execute_limit(static_cast<const PhysicalLimit &>(node), catalog, storage, index_manager);
    }

    return std::unexpected(make_error(ExecutionErrorCode::InvalidPlan, node.location(), "Unknown physical plan node"));
}

[[nodiscard]]
std::expected<ExecutionResult, ExecutionError> execute_query(
    const PhysicalQueryPlan & plan,
    meta::MetaEngine & catalog,
    storage::StorageEngine & storage,
    index::IndexManager & index_manager
)
{
    auto pipeline = execute_physical(plan.root(), catalog, storage, index_manager);
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
std::expected<ExecutionResult, ExecutionError> execute_use(const PhysicalUsePlan & plan)
{
    ExecutionResult result;
    result.kind = ExecutionResultKind::UseDatabase;
    result.selected_database_id = plan.database_id();
    result.selected_database_name = plan.database_name();
    return result;
}

[[nodiscard]]
std::expected<ExecutionResult, ExecutionError> execute_insert(
    const PhysicalInsertPlan & plan,
    storage::StorageEngine & storage,
    index::IndexManager & index_manager
)
{
    auto collection_storage = find_storage(storage, plan.collection_id(), plan.location());
    if (!collection_storage.has_value()) {
        return std::unexpected(std::move(collection_storage.error()));
    }

    evaluator::ExpressionEvaluator evaluator;
    const auto empty_record = make_empty_record();
    schema::RecordData record_data;
    record_data.values.reserve(plan.values().size());
    for (const auto & expression : plan.values()) {
        auto value = evaluator.evaluate(*expression, empty_record);
        if (!value.has_value()) {
            return std::unexpected(from_evaluation_error(std::move(value.error())));
        }
        record_data.values.push_back(std::move(value.value()));
    }

    auto index_bindings = index_manager.prepare_insert(plan.collection_id(), record_data);
    if (!index_bindings.has_value()) {
        return std::unexpected(from_index_error(std::move(index_bindings.error()), plan.location()));
    }

    auto inserted = storage.insert(plan.collection_id(), std::move(record_data));
    if (!inserted.has_value()) {
        return std::unexpected(from_storage_error(std::move(inserted.error()), plan.location()));
    }

    auto indexed = index_manager.on_insert(inserted.value(), index_bindings.value());
    if (!indexed.has_value()) {
        (void) storage.erase(plan.collection_id(), inserted.value());
        return std::unexpected(from_index_error(std::move(indexed.error()), plan.location()));
    }

    return command_result(1);
}

[[nodiscard]]
std::expected<ExecutionResult, ExecutionError> execute_delete(
    const PhysicalDeletePlan & plan,
    meta::MetaEngine & catalog,
    storage::StorageEngine & storage,
    index::IndexManager & index_manager
)
{
    auto rows = execute_physical(plan.input(), catalog, storage, index_manager);
    if (!rows.has_value()) {
        return std::unexpected(std::move(rows.error()));
    }

    auto collection_storage = find_storage(storage, plan.collection_id(), plan.location());
    if (!collection_storage.has_value()) {
        return std::unexpected(std::move(collection_storage.error()));
    }

    std::size_t affected_rows = 0;
    for (const auto & row : rows->rows) {
        auto index_bindings = index_manager.prepare_delete(plan.collection_id(), row.source_record.data);
        if (!index_bindings.has_value()) {
            return std::unexpected(from_index_error(std::move(index_bindings.error()), plan.location()));
        }

        auto erased = storage.erase(plan.collection_id(), row.source_record.record_id);
        if (!erased.has_value()) {
            return std::unexpected(from_storage_error(std::move(erased.error()), plan.location()));
        }

        auto indexed = index_manager.on_delete(row.source_record.record_id, index_bindings.value());
        if (!indexed.has_value()) {
            return std::unexpected(from_index_error(std::move(indexed.error()), plan.location()));
        }
        ++affected_rows;
    }

    return command_result(affected_rows);
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
    const PhysicalUpdatePlan & plan,
    meta::MetaEngine & catalog,
    storage::StorageEngine & storage,
    index::IndexManager & index_manager
)
{
    auto collection_schema = load_schema(catalog, plan.collection_id(), plan.location());
    if (!collection_schema.has_value()) {
        return std::unexpected(std::move(collection_schema.error()));
    }

    auto rows = execute_physical(plan.input(), catalog, storage, index_manager);
    if (!rows.has_value()) {
        return std::unexpected(std::move(rows.error()));
    }

    auto collection_storage = find_storage(storage, plan.collection_id(), plan.location());
    if (!collection_storage.has_value()) {
        return std::unexpected(std::move(collection_storage.error()));
    }

    evaluator::ExpressionEvaluator evaluator;
    std::size_t affected_rows = 0;
    for (const auto & row : rows->rows) {
        auto record_data = row.source_record.data;

        for (const auto & assignment : plan.assignments()) {
            auto ordinal = ordinal_for_column(collection_schema.value(), assignment.column.column_id);
            if (!ordinal.has_value() || ordinal.value() >= record_data.values.size()) {
                return std::unexpected(make_error(
                    ExecutionErrorCode::InvalidPlan,
                    plan.location(),
                    "Assignment column is not in collection schema"
                ));
            }

            auto value = evaluator.evaluate(*assignment.value, row.evaluation_record);
            if (!value.has_value()) {
                return std::unexpected(from_evaluation_error(std::move(value.error())));
            }
            record_data.values[ordinal.value()] = std::move(value.value());
        }

        auto index_bindings = index_manager.prepare_update(plan.collection_id(), row.source_record.data, record_data);
        if (!index_bindings.has_value()) {
            return std::unexpected(from_index_error(std::move(index_bindings.error()), plan.location()));
        }

        auto updated = storage.update(plan.collection_id(), row.source_record.record_id, std::move(record_data));
        if (!updated.has_value()) {
            return std::unexpected(from_storage_error(std::move(updated.error()), plan.location()));
        }

        auto indexed = index_manager.on_update(row.source_record.record_id, index_bindings.value());
        if (!indexed.has_value()) {
            (void) storage.update(plan.collection_id(), row.source_record.record_id, row.source_record.data);
            return std::unexpected(from_index_error(std::move(indexed.error()), plan.location()));
        }
        ++affected_rows;
    }

    return command_result(affected_rows);
}

[[nodiscard]]
std::expected<ExecutionResult, ExecutionError> execute_show_databases(meta::MetaEngine & catalog)
{
    std::vector<ExecutionRow> rows;
    for (const auto * database : catalog.list_databases()) {
        if (database != nullptr) {
            rows.push_back(ExecutionRow {.values = {schema::Value {database->name()}}});
        }
    }

    return rowset_result(
        {ExecutionColumn {.name = "database_name", .type = type(LogicalTypeId::Varchar)}},
        std::move(rows)
    );
}

[[nodiscard]]
std::expected<ExecutionResult, ExecutionError> execute_show_collections(
    const PhysicalShowCollectionsPlan & plan,
    meta::MetaEngine & catalog
)
{
    std::vector<ExecutionRow> rows;
    for (const auto * collection : catalog.list_collections(plan.database_id())) {
        if (collection != nullptr) {
            rows.push_back(ExecutionRow {.values = {schema::Value {collection->name()}}});
        }
    }

    return rowset_result(
        {ExecutionColumn {.name = "collection_name", .type = type(LogicalTypeId::Varchar)}},
        std::move(rows)
    );
}

[[nodiscard]]
std::expected<ExecutionResult, ExecutionError> execute_show_indexes(
    const PhysicalShowIndexesPlan & plan,
    meta::MetaEngine & catalog
)
{
    std::vector<ExecutionRow> rows;
    for (const auto * index : catalog.list_indexes(plan.collection_id())) {
        if (index == nullptr) {
            continue;
        }

        const auto column_id = index->column_id();
        const auto * column = column_id.has_value() ? catalog.find_column(column_id.value()) : nullptr;
        rows.push_back(ExecutionRow {
            .values = {
                schema::Value {index->name()},
                column != nullptr ? schema::Value {column->name()} : schema::Value::null(),
                schema::Value {index_kind_name(index->kind())},
                schema::Value {index->unique()},
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
    const PhysicalShowVectorIndexesPlan & plan,
    meta::MetaEngine & catalog
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
                schema::Value {index->name()},
                column != nullptr ? schema::Value {column->name()} : schema::Value::null(),
                schema::Value {vector_index_kind_name(index->index_kind())},
                schema::Value {vector_metric_name(index->metric())},
                schema::Value {static_cast<std::int64_t>(index->dimension())},
                schema::Value {static_cast<std::int64_t>(index->max_neighbors())},
                schema::Value {static_cast<std::int64_t>(index->ef_construction())},
                schema::Value {static_cast<std::int64_t>(index->ef_search_default())},
                schema::Value {static_cast<std::int64_t>(index->random_seed())},
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
    const PhysicalDescribeCollectionPlan & plan,
    meta::MetaEngine & catalog
)
{
    auto collection_schema = load_schema(catalog, plan.collection_id(), plan.location());
    if (!collection_schema.has_value()) {
        return std::unexpected(std::move(collection_schema.error()));
    }

    std::vector<ExecutionRow> rows;
    for (const auto & column : collection_schema->columns()) {
        rows.push_back(ExecutionRow {
            .values = {
                schema::Value {column.column_name()},
                schema::Value {logical_type_name(column.type())},
                schema::Value {column.nullable()},
                schema::Value {column.unique()},
                column.comment().has_value() ? schema::Value {column.comment().value()} : schema::Value::null(),
                collection_schema->comment().has_value() ? schema::Value {collection_schema->comment().value()} : schema::Value::null(),
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
    meta::MetaEngine & catalog,
    storage::StorageEngine & storage,
    index::IndexManager & index_manager
) noexcept
    : catalog_(catalog)
    , storage_(storage)
    , index_manager_(index_manager)
{
}

std::expected<ExecutionResult, ExecutionError> Executor::execute(const PhysicalStatementPlan & plan)
{
    switch (plan.kind()) {
    case PhysicalStatementPlanKind::Use:
        return execute_use(static_cast<const PhysicalUsePlan &>(plan));
    case PhysicalStatementPlanKind::CreateDatabase:
    case PhysicalStatementPlanKind::CreateCollection:
    case PhysicalStatementPlanKind::CreateIndex:
    case PhysicalStatementPlanKind::CreateVectorIndex:
    case PhysicalStatementPlanKind::DropDatabase:
    case PhysicalStatementPlanKind::DropCollection:
    case PhysicalStatementPlanKind::DropIndex:
    case PhysicalStatementPlanKind::DropVectorIndex:
        return std::unexpected(make_error(ExecutionErrorCode::UnsupportedStatement, plan.location(), "DDL must be executed by DatabaseEngine"));
    case PhysicalStatementPlanKind::ShowDatabases:
        return execute_show_databases(catalog_);
    case PhysicalStatementPlanKind::ShowCollections:
        return execute_show_collections(static_cast<const PhysicalShowCollectionsPlan &>(plan), catalog_);
    case PhysicalStatementPlanKind::ShowIndexes:
        return execute_show_indexes(static_cast<const PhysicalShowIndexesPlan &>(plan), catalog_);
    case PhysicalStatementPlanKind::ShowVectorIndexes:
        return execute_show_vector_indexes(static_cast<const PhysicalShowVectorIndexesPlan &>(plan), catalog_);
    case PhysicalStatementPlanKind::DescribeCollection:
        return execute_describe_collection(static_cast<const PhysicalDescribeCollectionPlan &>(plan), catalog_);
    case PhysicalStatementPlanKind::Insert:
        return execute_insert(static_cast<const PhysicalInsertPlan &>(plan), storage_, index_manager_);
    case PhysicalStatementPlanKind::Update:
        return execute_update(static_cast<const PhysicalUpdatePlan &>(plan), catalog_, storage_, index_manager_);
    case PhysicalStatementPlanKind::Delete:
        return execute_delete(static_cast<const PhysicalDeletePlan &>(plan), catalog_, storage_, index_manager_);
    case PhysicalStatementPlanKind::Query:
        return execute_query(static_cast<const PhysicalQueryPlan &>(plan), catalog_, storage_, index_manager_);
    }

    return std::unexpected(make_error(ExecutionErrorCode::UnsupportedStatement, internal_location, "Unsupported statement"));
}

} // namespace litedb::core::executor
