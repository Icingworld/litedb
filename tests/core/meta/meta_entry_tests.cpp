#include "core/common/logical_type.hpp"
#include "core/meta/entry/collection_entry.hpp"
#include "core/meta/entry/column_entry.hpp"
#include "core/meta/entry/database_entry.hpp"
#include "core/meta/entry/default_expression.hpp"
#include "core/meta/entry/index_entry.hpp"
#include "core/meta/entry/meta_entry.hpp"
#include "core/meta/entry/vector_index_entry.hpp"
#include "core/meta/meta_helper.hpp"

#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>

namespace
{

using namespace litedb::core;
using namespace litedb::core::meta::entry;

void require(bool condition, const char * message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

common::LogicalType type(common::LogicalTypeId id, std::optional<std::size_t> parameter = std::nullopt)
{
    return common::LogicalType {id, parameter};
}

void test_database_entry_tracks_collections()
{
    DatabaseEntry database {1, "Demo"};
    require(database.kind() == MetaEntryKind::Database, "database kind mismatch");
    require(database.id() == 1, "database id mismatch");
    require(database.name() == "Demo", "database name mismatch");
    require(database.key() == "demo", "database key mismatch");

    const auto users_key = meta::normalize_identifier("Users");
    database.add_collection(users_key, 10);
    database.add_collection(users_key, 10);

    require(database.contains_collection("users"), "database collection contains failed");
    require(database.find_collection_id("users").value() == 10, "database collection lookup mismatch");
    require(database.collection_ids().size() == 1, "database duplicate collection append mismatch");

    database.remove_collection("users", 10);
    require(!database.contains_collection("users"), "database collection remove failed");
    require(database.collection_ids().empty(), "database collection ids after remove mismatch");
}

void test_collection_entry_tracks_children()
{
    CollectionEntry collection {10, 1, "Users", "user collection"};
    require(collection.kind() == MetaEntryKind::Collection, "collection kind mismatch");
    require(collection.id() == 10, "collection id mismatch");
    require(collection.database_id() == 1, "collection database id mismatch");
    require(collection.comment().value() == "user collection", "collection comment mismatch");

    collection.add_column("id", 100);
    collection.add_column("name", 101);
    collection.add_column("id", 100);
    collection.add_index("idx_users_name", 200);
    collection.add_vector_index("vidx_embedding", 300);

    require(collection.column_ids().size() == 2, "collection column order size mismatch");
    require(collection.find_column_id("id").value() == 100, "collection column lookup mismatch");
    require(collection.find_index_id("idx_users_name").value() == 200, "collection index lookup mismatch");
    require(collection.find_vector_index_id("vidx_embedding").value() == 300, "collection vector index lookup mismatch");

    collection.remove_column("id", 100);
    collection.remove_index("idx_users_name", 200);
    collection.remove_vector_index("vidx_embedding", 300);

    require(!collection.contains_column("id"), "collection column remove failed");
    require(collection.index_ids().empty(), "collection index remove mismatch");
    require(collection.vector_index_ids().empty(), "collection vector index remove mismatch");
}

void test_column_entry_constraints_and_defaults()
{
    auto default_value = DefaultExpression::literal(DefaultLiteralKind::String, "unknown");
    ColumnEntry id {
        100,
        10,
        0,
        "Id",
        type(common::LogicalTypeId::BigInt),
        false,
        true,
        std::nullopt,
        "identifier"
    };
    ColumnEntry name {
        101,
        10,
        1,
        "name",
        type(common::LogicalTypeId::Varchar, 64),
        true,
        false,
        default_value,
        std::nullopt
    };

    require(id.kind() == MetaEntryKind::Column, "column kind mismatch");
    require(id.id() == 100, "column id mismatch");
    require(id.collection_id() == 10, "column collection id mismatch");
    require(id.ordinal() == 0, "column ordinal mismatch");
    require(!id.unique(), "column unique mismatch");
    require(id.nullable(), "column nullable mismatch");
    require(id.comment().value() == "identifier", "column comment mismatch");

    require(name.type().id == common::LogicalTypeId::Varchar, "column type mismatch");
    require(name.type().parameter.value() == 64, "column type parameter mismatch");
    require(name.default_expression().has_value(), "column default missing");
    require(name.default_expression()->value == "unknown", "column default value mismatch");
}

void test_index_entries()
{
    IndexEntry index {200, 10, {100, 101}, "idx_users_name", IndexKind::BTree, true};
    require(static_cast<const MetaEntry &>(index).kind() == MetaEntryKind::Index, "index entry kind mismatch");
    require(index.id() == 200, "index id mismatch");
    require(index.collection_id() == 10, "index collection id mismatch");
    require(index.column_id().value() == 100, "index first column mismatch");
    require(index.column_ids().size() == 2, "index columns size mismatch");
    require(index.kind() == IndexKind::BTree, "index kind mismatch");
    require(index.unique(), "index unique mismatch");
}

void test_vector_index_entry()
{
    HnswOptions options {
        .max_neighbors = 24,
        .ef_construction = 240,
        .ef_search_default = 80,
        .random_seed = 7,
    };
    VectorIndexEntry index {
        300,
        10,
        102,
        "vidx_embedding",
        VectorIndexKind::Hnsw,
        VectorDistanceMetric::Cosine,
        768,
        options
    };

    require(static_cast<const MetaEntry &>(index).kind() == MetaEntryKind::VectorIndex, "vector index entry kind mismatch");
    require(index.id() == 300, "vector index id mismatch");
    require(index.collection_id() == 10, "vector index collection id mismatch");
    require(index.column_id() == 102, "vector index column id mismatch");
    require(index.index_kind() == VectorIndexKind::Hnsw, "vector index kind mismatch");
    require(index.metric() == VectorDistanceMetric::Cosine, "vector index metric mismatch");
    require(index.dimension() == 768, "vector index dimension mismatch");
    require(index.max_neighbors() == 24, "vector index max_neighbors mismatch");
    require(index.ef_construction() == 240, "vector index ef_construction mismatch");
    require(index.ef_search_default() == 80, "vector index ef_search_default mismatch");
    require(index.random_seed() == 7, "vector index random seed mismatch");
}

void test_default_expression_vector()
{
    auto expression = DefaultExpression::vector({
        DefaultExpression::literal(DefaultLiteralKind::Float, "0.1"),
        DefaultExpression::literal(DefaultLiteralKind::Float, "0.2"),
    });

    require(expression.kind == DefaultExpressionKind::Vector, "default vector kind mismatch");
    require(expression.elements.size() == 2, "default vector size mismatch");
    require(expression.elements[0].literal_kind == DefaultLiteralKind::Float, "default vector literal mismatch");
    require(expression.elements[1].value == "0.2", "default vector value mismatch");
}

} // namespace

int main()
{
    try {
        test_database_entry_tracks_collections();
        test_collection_entry_tracks_children();
        test_column_entry_constraints_and_defaults();
        test_index_entries();
        test_vector_index_entry();
        test_default_expression_vector();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
