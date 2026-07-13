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
std::expected<storage::CollectionStorage *, ExecutionError> find_storage(
    storage::StorageManager & storage,
    common::CollectionId collection_id,
    AstNodeLocation location
)
{
    auto * collection_storage = storage.find_collection(collection_id);
    if (collection_storage == nullptr) {
        return std::unexpected(make_error(
            ExecutionErrorCode::CollectionStorageNotFound,
            location,
            "Collection storage not found"
        ));
    }
    return collection_storage;
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
    storage::StorageManager & storage,
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
    storage::StorageManager & storage
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

    auto cursor = collection_storage.value()->scan();
    while (auto record = cursor->next()) {
        append_pipeline_row(result, collection_schema.value(), std::move(record.value()));
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
    storage::StorageManager & storage,
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
        auto record = collection_storage.value()->get(record_id);
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
    storage::StorageManager & storage,
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
    storage::StorageManager & storage,
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
    storage::StorageManager & storage,
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
    storage::StorageManager & storage,
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
    storage::StorageManager & storage,
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
    storage::StorageManager & storage,
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
std::expected<ExecutionResult, ExecutionError> execute_create_database(
    const PhysicalCreateDatabasePlan & plan,
    meta::MetaEngine & catalog
)
{
    const auto existed = catalog.find_database(plan.database_name()) != nullptr;
    auto created = catalog.create_database(meta::CreateDatabaseRequest {
        .name = plan.database_name(),
        .if_not_exists = plan.if_not_exists(),
    });
    if (!created.has_value()) {
        return std::unexpected(from_meta_error(std::move(created.error()), plan.location()));
    }
    return command_result(existed ? 0 : 1);
}

[[nodiscard]]
std::expected<ExecutionResult, ExecutionError> execute_create_collection(
    const PhysicalCreateCollectionPlan & plan,
    meta::MetaEngine & catalog,
    storage::StorageManager & storage
)
{
    const auto * existing = catalog.find_collection(plan.database_id(), plan.collection_name());
    auto created = catalog.create_collection(meta::CreateCollectionRequest {
        .database_id = plan.database_id(),
        .name = plan.collection_name(),
        .if_not_exists = plan.if_not_exists(),
        .columns = plan.columns(),
        .comment = plan.comment(),
    });
    if (!created.has_value()) {
        return std::unexpected(from_meta_error(std::move(created.error()), plan.location()));
    }

    const auto collection_id = created.value();
    if (storage.find_collection(collection_id) == nullptr) {
        auto collection_schema = load_schema(catalog, collection_id, plan.location());
        if (!collection_schema.has_value()) {
            return std::unexpected(std::move(collection_schema.error()));
        }

        auto storage_result = storage.create_collection(std::move(collection_schema.value()));
        if (!storage_result.has_value()) {
            return std::unexpected(from_storage_error(std::move(storage_result.error()), plan.location()));
        }
    }

    return command_result(existing == nullptr ? 1 : 0);
}

[[nodiscard]]
std::expected<ExecutionResult, ExecutionError> execute_create_index(
    const PhysicalCreateIndexPlan & plan,
    meta::MetaEngine & catalog,
    storage::StorageManager & storage,
    index::IndexManager & index_manager
)
{
    const auto * existing = catalog.find_index(plan.collection_id(), plan.index_name());
    auto created = catalog.create_index(meta::CreateIndexRequest {
        .collection_id = plan.collection_id(),
        .column_ids = {plan.column_id()},
        .name = plan.index_name(),
        .kind = plan.index_kind(),
        .unique = plan.unique(),
        .if_not_exists = plan.if_not_exists(),
    });
    if (!created.has_value()) {
        return std::unexpected(from_meta_error(std::move(created.error()), plan.location()));
    }

    if (existing != nullptr) {
        return command_result(0);
    }

    const auto * index_entry = catalog.find_index(created.value());
    if (index_entry == nullptr) {
        return std::unexpected(make_error(
            ExecutionErrorCode::MetaError,
            plan.location(),
            "Created index metadata not found"
        ));
    }

    auto collection_schema = load_schema(catalog, plan.collection_id(), plan.location());
    if (!collection_schema.has_value()) {
        (void) catalog.drop_index(meta::DropIndexRequest {
            .collection_id = plan.collection_id(),
            .name = plan.index_name(),
            .if_exists = true,
        });
        return std::unexpected(std::move(collection_schema.error()));
    }

    auto collection_storage = find_storage(storage, plan.collection_id(), plan.location());
    if (!collection_storage.has_value()) {
        (void) catalog.drop_index(meta::DropIndexRequest {
            .collection_id = plan.collection_id(),
            .name = plan.index_name(),
            .if_exists = true,
        });
        return std::unexpected(std::move(collection_storage.error()));
    }

    auto created_index = index_manager.create_index(*index_entry, collection_schema.value(), *collection_storage.value());
    if (!created_index.has_value()) {
        (void) catalog.drop_index(meta::DropIndexRequest {
            .collection_id = plan.collection_id(),
            .name = plan.index_name(),
            .if_exists = true,
        });
        return std::unexpected(from_index_error(std::move(created_index.error()), plan.location()));
    }

    return command_result(existing == nullptr ? 1 : 0);
}

[[nodiscard]]
std::expected<ExecutionResult, ExecutionError> execute_create_vector_index(
    const PhysicalCreateVectorIndexPlan & plan,
    meta::MetaEngine & catalog
)
{
    const auto * existing = catalog.find_vector_index(plan.collection_id(), plan.index_name());
    auto created = catalog.create_vector_index(meta::CreateVectorIndexRequest {
        .collection_id = plan.collection_id(),
        .column_id = plan.column_id(),
        .name = plan.index_name(),
        .kind = plan.index_kind(),
        .metric = plan.metric(),
        .hnsw_options = {
            .max_neighbors = plan.max_neighbors(),
            .ef_construction = plan.ef_construction(),
            .ef_search_default = plan.ef_search_default(),
            .random_seed = plan.random_seed(),
        },
        .if_not_exists = plan.if_not_exists(),
    });
    if (!created.has_value()) {
        return std::unexpected(from_meta_error(std::move(created.error()), plan.location()));
    }
    return command_result(existing == nullptr ? 1 : 0);
}

[[nodiscard]]
std::expected<ExecutionResult, ExecutionError> execute_drop_collection(
    const PhysicalDropCollectionPlan & plan,
    meta::MetaEngine & catalog,
    storage::StorageManager & storage,
    index::IndexManager & index_manager
)
{
    if (!plan.collection_id().has_value()) {
        return command_result(0);
    }

    if (storage.find_collection(plan.collection_id().value()) != nullptr) {
        auto dropped_storage = storage.drop_collection(plan.collection_id().value());
        if (!dropped_storage.has_value()) {
            return std::unexpected(from_storage_error(std::move(dropped_storage.error()), plan.location()));
        }
    }

    auto dropped_catalog = catalog.drop_collection(meta::DropCollectionRequest {
        .database_id = plan.database_id(),
        .name = plan.collection_name(),
        .if_exists = plan.if_exists(),
    });
    if (!dropped_catalog.has_value()) {
        return std::unexpected(from_meta_error(std::move(dropped_catalog.error()), plan.location()));
    }

    index_manager.drop_collection_indexes(plan.collection_id().value());

    return command_result(1);
}

[[nodiscard]]
std::expected<ExecutionResult, ExecutionError> execute_drop_index(
    const PhysicalDropIndexPlan & plan,
    meta::MetaEngine & catalog,
    index::IndexManager & index_manager
)
{
    const auto * existing = catalog.find_index(plan.collection_id(), plan.index_name());
    if (existing == nullptr && plan.if_exists()) {
        return command_result(0);
    }
    const auto index_id = existing != nullptr ? std::optional<common::IndexId> {existing->id()} : std::nullopt;

    auto dropped = catalog.drop_index(meta::DropIndexRequest {
        .collection_id = plan.collection_id(),
        .name = plan.index_name(),
        .if_exists = plan.if_exists(),
    });
    if (!dropped.has_value()) {
        return std::unexpected(from_meta_error(std::move(dropped.error()), plan.location()));
    }

    if (index_id.has_value()) {
        auto dropped_index = index_manager.drop_index(index_id.value());
        if (!dropped_index.has_value() && dropped_index.error().code != index::IndexErrorCode::IndexNotFound) {
            return std::unexpected(from_index_error(std::move(dropped_index.error()), plan.location()));
        }
    }

    return command_result(1);
}

[[nodiscard]]
std::expected<ExecutionResult, ExecutionError> execute_drop_vector_index(
    const PhysicalDropVectorIndexPlan & plan,
    meta::MetaEngine & catalog
)
{
    const auto * existing = catalog.find_vector_index(plan.collection_id(), plan.index_name());
    if (existing == nullptr && plan.if_exists()) {
        return command_result(0);
    }

    auto dropped = catalog.drop_vector_index(meta::DropVectorIndexRequest {
        .collection_id = plan.collection_id(),
        .name = plan.index_name(),
        .if_exists = plan.if_exists(),
    });
    if (!dropped.has_value()) {
        return std::unexpected(from_meta_error(std::move(dropped.error()), plan.location()));
    }

    return command_result(existing == nullptr ? 0 : 1);
}

[[nodiscard]]
std::expected<ExecutionResult, ExecutionError> execute_drop_database(
    const PhysicalDropDatabasePlan & plan,
    meta::MetaEngine & catalog,
    storage::StorageManager & storage,
    index::IndexManager & index_manager
)
{
    if (!plan.database_id().has_value()) {
        return command_result(0);
    }

    const auto collections = catalog.list_collections(plan.database_id().value());
    std::vector<common::CollectionId> collection_ids;
    collection_ids.reserve(collections.size());
    for (const auto * collection : collections) {
        if (collection != nullptr) {
            collection_ids.push_back(collection->id());
        }
        if (collection != nullptr && storage.find_collection(collection->id()) != nullptr) {
            auto dropped = storage.drop_collection(collection->id());
            if (!dropped.has_value()) {
                return std::unexpected(from_storage_error(std::move(dropped.error()), plan.location()));
            }
        }
    }

    auto dropped_catalog = catalog.drop_database(meta::DropDatabaseRequest {
        .name = plan.database_name(),
        .if_exists = plan.if_exists(),
    });
    if (!dropped_catalog.has_value()) {
        return std::unexpected(from_meta_error(std::move(dropped_catalog.error()), plan.location()));
    }

    for (const auto collection_id : collection_ids) {
        index_manager.drop_collection_indexes(collection_id);
    }

    return command_result(1);
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
    storage::StorageManager & storage,
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

    auto inserted = collection_storage.value()->insert(std::move(record_data));
    if (!inserted.has_value()) {
        return std::unexpected(from_storage_error(std::move(inserted.error()), plan.location()));
    }

    auto indexed = index_manager.on_insert(inserted.value(), index_bindings.value());
    if (!indexed.has_value()) {
        (void) collection_storage.value()->erase(inserted.value());
        return std::unexpected(from_index_error(std::move(indexed.error()), plan.location()));
    }

    return command_result(1);
}

[[nodiscard]]
std::expected<ExecutionResult, ExecutionError> execute_delete(
    const PhysicalDeletePlan & plan,
    meta::MetaEngine & catalog,
    storage::StorageManager & storage,
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

        auto erased = collection_storage.value()->erase(row.source_record.record_id);
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
    storage::StorageManager & storage,
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

        auto updated = collection_storage.value()->update(row.source_record.record_id, std::move(record_data));
        if (!updated.has_value()) {
            return std::unexpected(from_storage_error(std::move(updated.error()), plan.location()));
        }

        auto indexed = index_manager.on_update(row.source_record.record_id, index_bindings.value());
        if (!indexed.has_value()) {
            (void) collection_storage.value()->update(row.source_record.record_id, row.source_record.data);
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
    storage::StorageManager & storage,
    index::IndexManager & index_manager,
    DdlMutationHandler * ddl_handler
) noexcept
    : catalog_(catalog)
    , storage_(storage)
    , index_manager_(index_manager)
    , ddl_handler_(ddl_handler)
{
}

std::expected<ExecutionResult, ExecutionError> Executor::execute(const PhysicalStatementPlan & plan)
{
    switch (plan.kind()) {
    case PhysicalStatementPlanKind::Use:
        return execute_use(static_cast<const PhysicalUsePlan &>(plan));
    case PhysicalStatementPlanKind::CreateDatabase:
        if (ddl_handler_ != nullptr) {
            return ddl_handler_->execute_create_database(static_cast<const PhysicalCreateDatabasePlan &>(plan), catalog_, storage_, index_manager_);
        }
        return execute_create_database(static_cast<const PhysicalCreateDatabasePlan &>(plan), catalog_);
    case PhysicalStatementPlanKind::CreateCollection:
        if (ddl_handler_ != nullptr) {
            return ddl_handler_->execute_create_collection(static_cast<const PhysicalCreateCollectionPlan &>(plan), catalog_, storage_, index_manager_);
        }
        return execute_create_collection(static_cast<const PhysicalCreateCollectionPlan &>(plan), catalog_, storage_);
    case PhysicalStatementPlanKind::CreateIndex:
        if (ddl_handler_ != nullptr) {
            return ddl_handler_->execute_create_index(static_cast<const PhysicalCreateIndexPlan &>(plan), catalog_, storage_, index_manager_);
        }
        return execute_create_index(static_cast<const PhysicalCreateIndexPlan &>(plan), catalog_, storage_, index_manager_);
    case PhysicalStatementPlanKind::CreateVectorIndex:
        if (ddl_handler_ != nullptr) {
            return ddl_handler_->execute_create_vector_index(static_cast<const PhysicalCreateVectorIndexPlan &>(plan), catalog_, storage_, index_manager_);
        }
        return execute_create_vector_index(static_cast<const PhysicalCreateVectorIndexPlan &>(plan), catalog_);
    case PhysicalStatementPlanKind::DropDatabase:
        if (ddl_handler_ != nullptr) {
            return ddl_handler_->execute_drop_database(static_cast<const PhysicalDropDatabasePlan &>(plan), catalog_, storage_, index_manager_);
        }
        return execute_drop_database(static_cast<const PhysicalDropDatabasePlan &>(plan), catalog_, storage_, index_manager_);
    case PhysicalStatementPlanKind::DropCollection:
        if (ddl_handler_ != nullptr) {
            return ddl_handler_->execute_drop_collection(static_cast<const PhysicalDropCollectionPlan &>(plan), catalog_, storage_, index_manager_);
        }
        return execute_drop_collection(static_cast<const PhysicalDropCollectionPlan &>(plan), catalog_, storage_, index_manager_);
    case PhysicalStatementPlanKind::DropIndex:
        if (ddl_handler_ != nullptr) {
            return ddl_handler_->execute_drop_index(static_cast<const PhysicalDropIndexPlan &>(plan), catalog_, storage_, index_manager_);
        }
        return execute_drop_index(static_cast<const PhysicalDropIndexPlan &>(plan), catalog_, index_manager_);
    case PhysicalStatementPlanKind::DropVectorIndex:
        if (ddl_handler_ != nullptr) {
            return ddl_handler_->execute_drop_vector_index(static_cast<const PhysicalDropVectorIndexPlan &>(plan), catalog_, storage_, index_manager_);
        }
        return execute_drop_vector_index(static_cast<const PhysicalDropVectorIndexPlan &>(plan), catalog_);
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
