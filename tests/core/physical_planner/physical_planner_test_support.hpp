#pragma once

#include <cstdint>
#include <exception>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/binder/bound/bound_order_by_item.hpp"
#include "core/binder/bound/bound_projection_item.hpp"
#include "core/binder/bound/expression/bound_binary_expression.hpp"
#include "core/binder/bound/expression/bound_column_ref_expression.hpp"
#include "core/binder/bound/expression/bound_function_expression.hpp"
#include "core/binder/bound/expression/bound_literal_expression.hpp"
#include "core/common/logical_type.hpp"
#include "core/common/value.hpp"
#include "core/function/builtin/builtin_functions.hpp"
#include "core/meta/meta_engine.hpp"

namespace physical_planner_test_support
{

using namespace litedb::core;
using namespace litedb::core::binder::bound;
using namespace litedb::core::common;
using namespace litedb::core::meta;

inline void require(bool condition, const char * message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

inline std::unique_ptr<BoundLiteralExpression> literal(LogicalTypeId id, Value value)
{
    return std::make_unique<BoundLiteralExpression>(LogicalType {id}, std::move(value));
}

inline std::unique_ptr<BoundLiteralExpression> integer_literal(std::int64_t value)
{
    return literal(LogicalTypeId::BigInt, Value {ValueData {value}});
}

inline std::unique_ptr<BoundLiteralExpression> boolean_literal(bool value)
{
    return literal(LogicalTypeId::Boolean, Value {ValueData {value}});
}

inline std::unique_ptr<BoundLiteralExpression> small_integer_literal(std::int32_t value)
{
    return literal(LogicalTypeId::Integer, Value {ValueData {value}});
}

inline std::unique_ptr<BoundColumnRefExpression> column_ref(
    const meta::entry::ColumnEntry & column
)
{
    return std::make_unique<BoundColumnRefExpression>(
        column.id(),
        column.ordinal(),
        column.type()
    );
}

struct PlannerCatalogFixture
{
    CatalogEditor editor;
    DatabaseId database_id {0};
    CollectionId collection_id {0};
    ColumnId age_id {0};
    ColumnId vector_id {0};
    IndexId first_age_index_id {0};
    VIndexId l2_index_id {0};
    VIndexId inner_product_index_id {0};
    VIndexId cosine_index_id {0};
};

inline PlannerCatalogFixture make_planner_catalog()
{
    CatalogEditor editor;
    auto database = editor.create_database(meta::CreateDatabaseRequest {.name = "planner_db"});
    require(database.has_value(), "planner fixture database creation failed");
    auto collection = editor.create_collection(meta::CreateCollectionRequest {
        .database_id = *database,
        .name = "planner_collection",
        .columns = {
            ColumnDefinition {.name = "id", .type = LogicalType {LogicalTypeId::BigInt, std::nullopt}},
            ColumnDefinition {.name = "age", .type = LogicalType {LogicalTypeId::Integer, std::nullopt}},
            ColumnDefinition {.name = "embedding", .type = LogicalType {LogicalTypeId::Vector, 3}},
        },
    });
    require(collection.has_value(), "planner fixture collection creation failed");

    const auto age = editor.view().find_column(*collection, "age");
    const auto embedding = editor.view().find_column(*collection, "embedding");
    require(age != nullptr && embedding != nullptr, "planner fixture columns missing");

    auto first_age_index = editor.create_index(meta::CreateIndexRequest {
        .collection_id = *collection,
        .column_ids = {age->id()},
        .name = "age_index_first",
    });
    require(first_age_index.has_value(), "planner fixture first scalar index creation failed");
    auto second_age_index = editor.create_index(meta::CreateIndexRequest {
        .collection_id = *collection,
        .column_ids = {age->id()},
        .name = "age_index_second",
    });
    require(second_age_index.has_value(), "planner fixture second scalar index creation failed");

    const auto make_vector_index = [&](std::string name, meta::entry::VectorDistanceMetric metric) {
        return editor.create_vector_index(meta::CreateVectorIndexRequest {
            .collection_id = *collection,
            .column_id = embedding->id(),
            .name = std::move(name),
            .metric = metric,
            .hnsw_options = meta::entry::HnswOptions {
                .max_neighbors = 16,
                .ef_construction = 32,
                .ef_search_default = 16,
                .random_seed = 1,
            },
        });
    };
    auto l2_index = make_vector_index("embedding_l2", meta::entry::VectorDistanceMetric::L2);
    auto inner_product_index = make_vector_index(
        "embedding_inner_product",
        meta::entry::VectorDistanceMetric::InnerProduct
    );
    auto cosine_index = make_vector_index("embedding_cosine", meta::entry::VectorDistanceMetric::Cosine);
    require(l2_index.has_value() && inner_product_index.has_value() && cosine_index.has_value(),
            "planner fixture vector index creation failed");

    return PlannerCatalogFixture {
        .editor = std::move(editor),
        .database_id = *database,
        .collection_id = *collection,
        .age_id = age->id(),
        .vector_id = embedding->id(),
        .first_age_index_id = *first_age_index,
        .l2_index_id = *l2_index,
        .inner_product_index_id = *inner_product_index,
        .cosine_index_id = *cosine_index,
    };
}

inline std::unique_ptr<BoundExpression> scalar_predicate(
    const meta::entry::ColumnEntry & column,
    BinaryOperator operation,
    std::unique_ptr<BoundExpression> value
)
{
    return std::make_unique<BoundBinaryExpression>(
        column_ref(column),
        operation,
        std::move(value),
        LogicalType {LogicalTypeId::Boolean, std::nullopt}
    );
}

inline std::unique_ptr<BoundFunctionExpression> vector_distance(
    const meta::entry::ColumnEntry & column,
    std::string_view name,
    std::vector<double> query
)
{
    const auto argument_types = std::vector<LogicalType> {column.type(), column.type()};
    auto function = function::builtin::builtin_function_catalog().bind_scalar(name, argument_types);
    require(function.has_value(), "vector distance function binding failed");

    std::vector<std::unique_ptr<BoundExpression>> arguments;
    arguments.push_back(column_ref(column));
    arguments.push_back(std::make_unique<BoundLiteralExpression>(
        column.type(),
        Value {ValueData {std::move(query)}}
    ));
    return std::make_unique<BoundFunctionExpression>(std::move(*function), std::move(arguments));
}

} // namespace physical_planner_test_support
