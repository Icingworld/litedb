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
    request.columns.push_back(ColumnDefinitionRequest {
        .name = "id",
        .type = type(LogicalTypeId::BigInt),
        .primary_key = true,
        .comment = "primary identifier",
    });
    request.columns.push_back(ColumnDefinitionRequest {
        .name = "name",
        .type = type(LogicalTypeId::Varchar, 64),
        .unique = true,
        .default_expression = CatalogDefaultExpression::literal(CatalogDefaultLiteralKind::String, "unknown"),
    });
    request.columns.push_back(ColumnDefinitionRequest {
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
        .columns = {ColumnDefinitionRequest {.name = "id", .type = type(LogicalTypeId::BigInt)}},
    });
    require(!duplicate_collection.has_value(), "duplicate collection should fail");
    require(duplicate_collection.error().code == CatalogErrorCode::DuplicateCollection, "duplicate collection error mismatch");

    auto if_not_exists = catalog.create_collection(CreateCollectionRequest {
        .database_id = database_id,
        .name = "Users",
        .if_not_exists = true,
        .columns = {ColumnDefinitionRequest {.name = "id", .type = type(LogicalTypeId::BigInt)}},
    });
    require(if_not_exists.has_value(), "IF NOT EXISTS collection should succeed");
    require(if_not_exists.value() == collection_id, "IF NOT EXISTS collection id mismatch");

    auto duplicate_column = catalog.create_collection(CreateCollectionRequest {
        .database_id = database_id,
        .name = "events",
        .columns = {
            ColumnDefinitionRequest {.name = "id", .type = type(LogicalTypeId::BigInt)},
            ColumnDefinitionRequest {.name = "ID", .type = type(LogicalTypeId::BigInt)},
        },
    });
    require(!duplicate_column.has_value(), "duplicate column should fail");
    require(duplicate_column.error().code == CatalogErrorCode::DuplicateColumn, "duplicate column error mismatch");

    auto multiple_primary_keys = catalog.create_collection(CreateCollectionRequest {
        .database_id = database_id,
        .name = "bad_keys",
        .columns = {
            ColumnDefinitionRequest {.name = "id", .type = type(LogicalTypeId::BigInt), .primary_key = true},
            ColumnDefinitionRequest {.name = "other_id", .type = type(LogicalTypeId::BigInt), .primary_key = true},
        },
    });
    require(!multiple_primary_keys.has_value(), "multiple primary keys should fail");
    require(multiple_primary_keys.error().code == CatalogErrorCode::MultiplePrimaryKeys, "multiple primary keys error mismatch");
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
        test_list_and_drop();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
