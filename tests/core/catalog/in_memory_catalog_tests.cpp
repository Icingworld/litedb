#include "core/catalog/in_memory_catalog.hpp"

#include <exception>
#include <iostream>
#include <stdexcept>

namespace
{

using namespace litedb::core::catalog;
using namespace litedb::core::common;

void require(bool condition, const char * message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

LogicalType type(LogicalTypeId id, std::optional<std::size_t> parameter = std::nullopt)
{
    return LogicalType {id, parameter};
}

DatabaseId create_database_ok(InMemoryCatalog & catalog, std::string name)
{
    auto result = catalog.create_database(CreateDatabaseRequest {.name = std::move(name)});
    if (!result.has_value()) {
        throw std::runtime_error(result.error().message);
    }
    return result.value();
}

CollectionId create_users_collection_ok(InMemoryCatalog & catalog, DatabaseId database_id)
{
    CreateCollectionRequest request;
    request.database_id = database_id;
    request.name = "users";
    request.columns.push_back(ColumnDefinition {
        .name = "id",
        .type = type(LogicalTypeId::BigInt),
        .primary_key = true,
        .comment = "primary identifier",
    });
    request.columns.push_back(ColumnDefinition {
        .name = "name",
        .type = type(LogicalTypeId::Varchar, 64),
        .unique = true,
        .default_expression = CatalogDefaultExpression::literal(CatalogDefaultLiteralKind::String, "unknown"),
    });
    request.columns.push_back(ColumnDefinition {
        .name = "embedding",
        .type = type(LogicalTypeId::Vector, 3),
        .default_expression = CatalogDefaultExpression::vector({
            CatalogDefaultExpression::literal(CatalogDefaultLiteralKind::Float, "0.1"),
            CatalogDefaultExpression::literal(CatalogDefaultLiteralKind::Float, "0.2"),
            CatalogDefaultExpression::literal(CatalogDefaultLiteralKind::Float, "0.3"),
        }),
    });

    auto result = catalog.create_collection(request);
    if (!result.has_value()) {
        throw std::runtime_error(result.error().message);
    }
    return result.value();
}

void test_create_database_case_insensitive_lookup()
{
    InMemoryCatalog catalog;
    const auto database_id = create_database_ok(catalog, "Demo");

    const auto * original = catalog.find_database("Demo");
    const auto * lower = catalog.find_database("demo");
    const auto * upper = catalog.find_database("DEMO");

    require(original != nullptr, "database lookup by original name failed");
    require(lower != nullptr, "database lookup by lowercase name failed");
    require(upper != nullptr, "database lookup by uppercase name failed");
    require(original->id() == database_id, "database id mismatch");
    require(original->name() == "Demo", "database original name mismatch");
    require(lower->id() == database_id, "case-insensitive database id mismatch");

    auto duplicate = catalog.create_database(CreateDatabaseRequest {.name = "demo"});
    require(!duplicate.has_value(), "duplicate database should fail");
    require(duplicate.error().code == CatalogErrorCode::DuplicateDatabase, "duplicate database error code mismatch");

    auto if_not_exists = catalog.create_database(CreateDatabaseRequest {.name = "DEMO", .if_not_exists = true});
    require(if_not_exists.has_value(), "IF NOT EXISTS database should succeed");
    require(if_not_exists.value() == database_id, "IF NOT EXISTS database id mismatch");
}

void test_create_collection_columns_and_defaults()
{
    InMemoryCatalog catalog;
    const auto database_id = create_database_ok(catalog, "demo");
    const auto collection_id = create_users_collection_ok(catalog, database_id);

    const auto * collection = catalog.find_collection(database_id, "USERS");
    require(collection != nullptr, "collection lookup failed");
    require(collection->id() == collection_id, "collection id mismatch");
    require(collection->name() == "users", "collection original name mismatch");

    const auto * id = catalog.find_column(collection_id, "ID");
    const auto * name = catalog.find_column(collection_id, "name");
    const auto * embedding = catalog.find_column(collection_id, "Embedding");

    require(id != nullptr, "id column lookup failed");
    require(name != nullptr, "name column lookup failed");
    require(embedding != nullptr, "embedding column lookup failed");
    require(id->primary_key(), "primary key mismatch");
    require(!id->nullable(), "primary key should be non-nullable");
    require(name->unique(), "unique constraint mismatch");
    require(name->type().id == LogicalTypeId::Varchar, "varchar type mismatch");
    require(name->type().parameter.value() == 64, "varchar length mismatch");
    require(name->default_expression().has_value(), "name default missing");
    require(embedding->type().id == LogicalTypeId::Vector, "vector type mismatch");
    require(embedding->type().parameter.value() == 3, "vector dimension mismatch");
    require(embedding->default_expression().has_value(), "vector default missing");
    require(embedding->default_expression()->elements.size() == 3, "vector default element count mismatch");

    const auto columns = catalog.list_columns(collection_id);
    require(columns.size() == 3, "column list size mismatch");
    require(columns[0]->name() == "id", "column order mismatch");
    require(columns[1]->name() == "name", "column order mismatch");
    require(columns[2]->name() == "embedding", "column order mismatch");
}

void test_duplicate_collection_and_column_rules()
{
    InMemoryCatalog catalog;
    const auto database_id = create_database_ok(catalog, "demo");
    const auto collection_id = create_users_collection_ok(catalog, database_id);

    auto duplicate_collection = catalog.create_collection(CreateCollectionRequest {
        .database_id = database_id,
        .name = "USERS",
        .columns = {ColumnDefinition {.name = "id", .type = type(LogicalTypeId::BigInt)}},
    });
    require(!duplicate_collection.has_value(), "duplicate collection should fail");
    require(duplicate_collection.error().code == CatalogErrorCode::DuplicateCollection, "duplicate collection error mismatch");

    auto if_not_exists = catalog.create_collection(CreateCollectionRequest {
        .database_id = database_id,
        .name = "Users",
        .if_not_exists = true,
        .columns = {ColumnDefinition {.name = "id", .type = type(LogicalTypeId::BigInt)}},
    });
    require(if_not_exists.has_value(), "IF NOT EXISTS collection should succeed");
    require(if_not_exists.value() == collection_id, "IF NOT EXISTS collection id mismatch");

    auto duplicate_column = catalog.create_collection(CreateCollectionRequest {
        .database_id = database_id,
        .name = "events",
        .columns = {
            ColumnDefinition {.name = "id", .type = type(LogicalTypeId::BigInt)},
            ColumnDefinition {.name = "ID", .type = type(LogicalTypeId::BigInt)},
        },
    });
    require(!duplicate_column.has_value(), "duplicate column should fail");
    require(duplicate_column.error().code == CatalogErrorCode::DuplicateColumn, "duplicate column error mismatch");

    auto multiple_primary_keys = catalog.create_collection(CreateCollectionRequest {
        .database_id = database_id,
        .name = "bad_keys",
        .columns = {
            ColumnDefinition {.name = "id", .type = type(LogicalTypeId::BigInt), .primary_key = true},
            ColumnDefinition {.name = "other_id", .type = type(LogicalTypeId::BigInt), .primary_key = true},
        },
    });
    require(!multiple_primary_keys.has_value(), "multiple primary keys should fail");
    require(multiple_primary_keys.error().code == CatalogErrorCode::MultiplePrimaryKeys, "multiple primary keys error mismatch");
}

void test_index_catalog_operations()
{
    InMemoryCatalog catalog;
    const auto database_id = create_database_ok(catalog, "demo");
    const auto collection_id = create_users_collection_ok(catalog, database_id);

    const auto * collection = catalog.find_collection(collection_id);
    require(collection != nullptr, "collection lookup for index test failed");
    const auto * id_column = catalog.find_column(collection_id, "id");
    const auto * name_column = catalog.find_column(collection_id, "name");
    const auto * embedding_column = catalog.find_column(collection_id, "embedding");
    require(id_column != nullptr, "id column lookup for index failed");
    require(name_column != nullptr, "name column lookup for index failed");
    require(embedding_column != nullptr, "embedding column lookup for index failed");

    auto id_index = catalog.create_index(CreateIndexRequest {
        .collection_id = collection_id,
        .column_id = id_column->id(),
        .name = "idx_id",
        .index_kind = CatalogIndexKind::BTree,
    });
    require(id_index.has_value(), "create btree index failed");

    auto name_index = catalog.create_index(CreateIndexRequest {
        .collection_id = collection_id,
        .column_id = name_column->id(),
        .name = "idx_name",
        .index_kind = CatalogIndexKind::Hash,
    });
    require(name_index.has_value(), "create hash index failed");

    const auto * found_id_index = catalog.find_index(collection_id, "IDX_ID");
    require(found_id_index != nullptr, "case-insensitive index lookup failed");
    require(found_id_index->id() == id_index.value(), "index id mismatch");
    require(found_id_index->collection_id() == collection_id, "index collection id mismatch");
    require(found_id_index->column_id() == id_column->id(), "index column id mismatch");
    require(found_id_index->index_kind() == CatalogIndexKind::BTree, "index kind mismatch");
    require(!found_id_index->unique(), "index unique flag mismatch");

    const auto indexes = catalog.list_indexes(collection_id);
    require(indexes.size() == 2, "index list size mismatch");
    require(indexes[0]->id() == id_index.value(), "index list order mismatch");
    require(indexes[1]->id() == name_index.value(), "index list order mismatch");

    auto duplicate = catalog.create_index(CreateIndexRequest {
        .collection_id = collection_id,
        .column_id = id_column->id(),
        .name = "IDX_ID",
        .index_kind = CatalogIndexKind::Hash,
    });
    require(!duplicate.has_value(), "duplicate index should fail");
    require(duplicate.error().code == CatalogErrorCode::DuplicateIndex, "duplicate index error mismatch");

    auto if_not_exists = catalog.create_index(CreateIndexRequest {
        .collection_id = collection_id,
        .column_id = id_column->id(),
        .name = "idx_id",
        .index_kind = CatalogIndexKind::Hash,
        .if_not_exists = true,
    });
    require(if_not_exists.has_value(), "IF NOT EXISTS index should succeed");
    require(if_not_exists.value() == id_index.value(), "IF NOT EXISTS index id mismatch");

    auto missing_column = catalog.create_index(CreateIndexRequest {
        .collection_id = collection_id,
        .column_id = 999,
        .name = "idx_missing",
    });
    require(!missing_column.has_value(), "missing index column should fail");
    require(missing_column.error().code == CatalogErrorCode::ColumnNotFound, "missing index column error mismatch");

    auto vector_index = catalog.create_index(CreateIndexRequest {
        .collection_id = collection_id,
        .column_id = embedding_column->id(),
        .name = "idx_embedding",
    });
    require(!vector_index.has_value(), "scalar index on vector column should fail");
    require(vector_index.error().code == CatalogErrorCode::InvalidArgument, "vector scalar index error mismatch");

    auto drop_missing = catalog.drop_index(DropIndexRequest {
        .collection_id = collection_id,
        .name = "missing",
    });
    require(!drop_missing.has_value(), "missing index drop should fail");
    require(drop_missing.error().code == CatalogErrorCode::IndexNotFound, "missing index drop error mismatch");

    auto drop_missing_if_exists = catalog.drop_index(DropIndexRequest {
        .collection_id = collection_id,
        .name = "missing",
        .if_exists = true,
    });
    require(drop_missing_if_exists.has_value(), "DROP INDEX IF EXISTS should succeed");

    auto drop_name = catalog.drop_index(DropIndexRequest {
        .collection_id = collection_id,
        .name = "IDX_NAME",
    });
    require(drop_name.has_value(), "drop index should succeed");
    require(catalog.find_index(name_index.value()) == nullptr, "dropped index should not be found by id");
    require(catalog.find_index(collection_id, "idx_name") == nullptr, "dropped index should not be found by name");
    require(catalog.list_indexes(collection_id).size() == 1, "index list after drop mismatch");

    auto drop_collection = catalog.drop_collection(DropCollectionRequest {
        .database_id = database_id,
        .name = "users",
    });
    require(drop_collection.has_value(), "drop indexed collection should succeed");
    require(catalog.find_index(id_index.value()) == nullptr, "collection drop should remove indexes");
}

void test_vector_index_catalog_operations()
{
    InMemoryCatalog catalog;
    const auto database_id = create_database_ok(catalog, "demo");
    const auto collection_id = create_users_collection_ok(catalog, database_id);

    const auto * id_column = catalog.find_column(collection_id, "id");
    const auto * embedding_column = catalog.find_column(collection_id, "embedding");
    require(id_column != nullptr, "id column lookup for vector index failed");
    require(embedding_column != nullptr, "embedding column lookup for vector index failed");

    auto vector_index = catalog.create_vector_index(CreateVectorIndexRequest {
        .collection_id = collection_id,
        .column_id = embedding_column->id(),
        .name = "vidx_embedding",
        .index_kind = CatalogVectorIndexKind::Hnsw,
        .metric = CatalogVectorDistanceMetric::Cosine,
        .max_neighbors = 24,
        .ef_construction = 240,
        .ef_search_default = 80,
        .random_seed = 7,
    });
    require(vector_index.has_value(), "create vector index failed");

    const auto * found = catalog.find_vector_index(collection_id, "VIDX_EMBEDDING");
    require(found != nullptr, "case-insensitive vector index lookup failed");
    require(found->id() == vector_index.value(), "vector index id mismatch");
    require(found->collection_id() == collection_id, "vector index collection id mismatch");
    require(found->column_id() == embedding_column->id(), "vector index column id mismatch");
    require(found->index_kind() == CatalogVectorIndexKind::Hnsw, "vector index kind mismatch");
    require(found->metric() == CatalogVectorDistanceMetric::Cosine, "vector index metric mismatch");
    require(found->dimension() == 3, "vector index dimension mismatch");
    require(found->max_neighbors() == 24, "vector index max_neighbors mismatch");
    require(found->ef_construction() == 240, "vector index ef_construction mismatch");
    require(found->ef_search_default() == 80, "vector index ef_search_default mismatch");
    require(found->random_seed() == 7, "vector index random seed mismatch");

    const auto indexes = catalog.list_vector_indexes(collection_id);
    require(indexes.size() == 1, "vector index list size mismatch");
    require(indexes[0]->id() == vector_index.value(), "vector index list order mismatch");

    auto duplicate = catalog.create_vector_index(CreateVectorIndexRequest {
        .collection_id = collection_id,
        .column_id = embedding_column->id(),
        .name = "VIDX_EMBEDDING",
    });
    require(!duplicate.has_value(), "duplicate vector index should fail");
    require(duplicate.error().code == CatalogErrorCode::DuplicateIndex, "duplicate vector index error mismatch");

    auto if_not_exists = catalog.create_vector_index(CreateVectorIndexRequest {
        .collection_id = collection_id,
        .column_id = embedding_column->id(),
        .name = "vidx_embedding",
        .if_not_exists = true,
    });
    require(if_not_exists.has_value(), "IF NOT EXISTS vector index should succeed");
    require(if_not_exists.value() == vector_index.value(), "IF NOT EXISTS vector index id mismatch");

    auto scalar_column = catalog.create_vector_index(CreateVectorIndexRequest {
        .collection_id = collection_id,
        .column_id = id_column->id(),
        .name = "vidx_id",
    });
    require(!scalar_column.has_value(), "vector index on scalar column should fail");
    require(scalar_column.error().code == CatalogErrorCode::InvalidArgument, "vector index scalar column error mismatch");

    auto bad_options = catalog.create_vector_index(CreateVectorIndexRequest {
        .collection_id = collection_id,
        .column_id = embedding_column->id(),
        .name = "vidx_bad",
        .max_neighbors = 16,
        .ef_construction = 8,
    });
    require(!bad_options.has_value(), "invalid HNSW options should fail");
    require(bad_options.error().code == CatalogErrorCode::InvalidArgument, "invalid HNSW options error mismatch");

    auto drop_missing = catalog.drop_vector_index(DropVectorIndexRequest {
        .collection_id = collection_id,
        .name = "missing",
    });
    require(!drop_missing.has_value(), "missing vector index drop should fail");
    require(drop_missing.error().code == CatalogErrorCode::IndexNotFound, "missing vector index drop error mismatch");

    auto drop_missing_if_exists = catalog.drop_vector_index(DropVectorIndexRequest {
        .collection_id = collection_id,
        .name = "missing",
        .if_exists = true,
    });
    require(drop_missing_if_exists.has_value(), "DROP VINDEX IF EXISTS should succeed");

    auto drop = catalog.drop_vector_index(DropVectorIndexRequest {
        .collection_id = collection_id,
        .name = "VIDX_EMBEDDING",
    });
    require(drop.has_value(), "drop vector index should succeed");
    require(catalog.find_vector_index(vector_index.value()) == nullptr, "dropped vector index should not be found by id");
    require(catalog.find_vector_index(collection_id, "vidx_embedding") == nullptr, "dropped vector index should not be found by name");
}

void test_index_snapshot_restore()
{
    InMemoryCatalog catalog;
    const auto database_id = create_database_ok(catalog, "demo");
    const auto collection_id = create_users_collection_ok(catalog, database_id);
    const auto * name_column = catalog.find_column(collection_id, "name");
    require(name_column != nullptr, "name column lookup for snapshot failed");

    auto index_id = catalog.create_index(CreateIndexRequest {
        .collection_id = collection_id,
        .column_id = name_column->id(),
        .name = "idx_name",
        .index_kind = CatalogIndexKind::Hash,
        .unique = true,
    });
    require(index_id.has_value(), "create snapshot index failed");

    InMemoryCatalog restored;
    auto restore = restored.restore(catalog.snapshot());
    require(restore.has_value(), "catalog restore with index failed");

    const auto * restored_index = restored.find_index(collection_id, "IDX_NAME");
    require(restored_index != nullptr, "restored index lookup failed");
    require(restored_index->id() == index_id.value(), "restored index id mismatch");
    require(restored_index->column_id() == name_column->id(), "restored index column mismatch");
    require(restored_index->index_kind() == CatalogIndexKind::Hash, "restored index kind mismatch");
    require(restored_index->unique(), "restored index unique mismatch");
}

void test_vector_index_snapshot_restore()
{
    InMemoryCatalog catalog;
    const auto database_id = create_database_ok(catalog, "demo");
    const auto collection_id = create_users_collection_ok(catalog, database_id);
    const auto * embedding_column = catalog.find_column(collection_id, "embedding");
    require(embedding_column != nullptr, "embedding column lookup for vector snapshot failed");

    auto index_id = catalog.create_vector_index(CreateVectorIndexRequest {
        .collection_id = collection_id,
        .column_id = embedding_column->id(),
        .name = "vidx_embedding",
        .metric = CatalogVectorDistanceMetric::InnerProduct,
        .max_neighbors = 32,
        .ef_construction = 256,
        .ef_search_default = 96,
        .random_seed = 11,
    });
    require(index_id.has_value(), "create snapshot vector index failed");

    InMemoryCatalog restored;
    auto restore = restored.restore(catalog.snapshot());
    require(restore.has_value(), "catalog restore with vector index failed");

    const auto * restored_index = restored.find_vector_index(collection_id, "VIDX_EMBEDDING");
    require(restored_index != nullptr, "restored vector index lookup failed");
    require(restored_index->id() == index_id.value(), "restored vector index id mismatch");
    require(restored_index->column_id() == embedding_column->id(), "restored vector index column mismatch");
    require(restored_index->metric() == CatalogVectorDistanceMetric::InnerProduct, "restored vector index metric mismatch");
    require(restored_index->dimension() == 3, "restored vector index dimension mismatch");
    require(restored_index->max_neighbors() == 32, "restored vector index max_neighbors mismatch");
    require(restored_index->ef_construction() == 256, "restored vector index ef_construction mismatch");
    require(restored_index->ef_search_default() == 96, "restored vector index ef_search_default mismatch");
    require(restored_index->random_seed() == 11, "restored vector index random seed mismatch");
}

void test_list_and_drop()
{
    InMemoryCatalog catalog;
    const auto demo_id = create_database_ok(catalog, "demo");
    const auto analytics_id = create_database_ok(catalog, "analytics");
    const auto users_id = create_users_collection_ok(catalog, demo_id);

    const auto databases = catalog.list_databases();
    require(databases.size() == 2, "database list size mismatch");
    require(databases[0]->id() == demo_id, "database list order mismatch");
    require(databases[1]->id() == analytics_id, "database list order mismatch");

    const auto collections = catalog.list_collections(demo_id);
    require(collections.size() == 1, "collection list size mismatch");
    require(collections[0]->id() == users_id, "collection list id mismatch");

    auto drop_missing_collection = catalog.drop_collection(DropCollectionRequest {
        .database_id = demo_id,
        .name = "missing",
    });
    require(!drop_missing_collection.has_value(), "missing collection drop should fail");
    require(drop_missing_collection.error().code == CatalogErrorCode::CollectionNotFound, "missing collection error mismatch");

    auto drop_missing_collection_if_exists = catalog.drop_collection(DropCollectionRequest {
        .database_id = demo_id,
        .name = "missing",
        .if_exists = true,
    });
    require(drop_missing_collection_if_exists.has_value(), "DROP COLLECTION IF EXISTS should succeed");

    auto drop_users = catalog.drop_collection(DropCollectionRequest {
        .database_id = demo_id,
        .name = "USERS",
    });
    require(drop_users.has_value(), "drop users should succeed");
    require(catalog.find_collection(users_id) == nullptr, "dropped collection should not be found by id");
    require(catalog.find_collection(demo_id, "users") == nullptr, "dropped collection should not be found by name");

    auto drop_demo = catalog.drop_database(DropDatabaseRequest {.name = "DEMO"});
    require(drop_demo.has_value(), "drop database should succeed");
    require(catalog.find_database(demo_id) == nullptr, "dropped database should not be found by id");

    auto drop_missing_database = catalog.drop_database(DropDatabaseRequest {.name = "demo"});
    require(!drop_missing_database.has_value(), "missing database drop should fail");
    require(drop_missing_database.error().code == CatalogErrorCode::DatabaseNotFound, "missing database error mismatch");

    auto drop_missing_database_if_exists = catalog.drop_database(DropDatabaseRequest {
        .name = "demo",
        .if_exists = true,
    });
    require(drop_missing_database_if_exists.has_value(), "DROP DATABASE IF EXISTS should succeed");
}

} // namespace

int main()
{
    try {
        test_create_database_case_insensitive_lookup();
        test_create_collection_columns_and_defaults();
        test_duplicate_collection_and_column_rules();
        test_index_catalog_operations();
        test_vector_index_catalog_operations();
        test_index_snapshot_restore();
        test_vector_index_snapshot_restore();
        test_list_and_drop();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
