#include "core/meta/meta_engine.hpp"
#include "core/storage/schema_loader.hpp"

#include <exception>
#include <iostream>
#include <stdexcept>

namespace
{

using namespace litedb::core::common;
using namespace litedb::core::meta;
using namespace litedb::core::meta::entry;
using namespace litedb::core::schema;
using namespace litedb::core::storage;

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

CollectionId create_users_collection(CatalogEditor & catalog)
{
    auto database = catalog.create_database(CreateDatabaseRequest {.name = "demo"});
    if (!database.has_value()) {
        throw std::runtime_error(std::string {database.error().message()});
    }

    CreateCollectionRequest request;
    request.database_id = *database;
    request.name = "users";
    request.comment = "user collection";
    request.columns.push_back(ColumnDefinition {
        .name = "id",
        .type = type(LogicalTypeId::BigInt),
        .nullable = false,
        .comment = "primary identifier",
    });
    request.columns.push_back(ColumnDefinition {
        .name = "name",
        .type = type(LogicalTypeId::Varchar, 64),
        .unique = true,
        .default_expression = DefaultExpression::literal(DefaultLiteralKind::String, "unknown"),
    });
    request.columns.push_back(ColumnDefinition {
        .name = "embedding",
        .type = type(LogicalTypeId::Vector, 3),
        .nullable = true,
    });

    auto collection = catalog.create_collection(request);
    if (!collection.has_value()) {
        throw std::runtime_error(std::string {collection.error().message()});
    }
    return *collection;
}

void test_load_collection_schema_from_catalog()
{
    CatalogEditor catalog;
    const auto collection_id = create_users_collection(catalog);

    auto loaded = load_collection_schema(catalog.view(), collection_id);
    require(loaded.has_value(), "collection schema load failed");

    const auto & schema = loaded.value();
    require(schema.collection_id() == collection_id, "collection id mismatch");
    require(schema.collection_name() == "users", "collection name mismatch");
    require(schema.comment().has_value(), "collection comment missing");
    require(schema.comment().value() == "user collection", "collection comment mismatch");
    require(schema.columns().size() == 3, "column count mismatch");

    const auto * id = schema.column_at(0);
    const auto * name = schema.find_column("NAME");
    const auto * embedding = schema.column_at(2);

    require(id != nullptr, "id column missing");
    require(name != nullptr, "name column missing");
    require(embedding != nullptr, "embedding column missing");

    require(id->ordinal() == 0, "id ordinal mismatch");
    require(id->column_name() == "id", "id column name mismatch");
    require(id->type().id == LogicalTypeId::BigInt, "id type mismatch");
    require(!id->nullable(), "id should be non-nullable");
    require(id->comment().has_value(), "comment missing");

    require(name->ordinal() == 1, "name ordinal mismatch");
    require(name->unique(), "unique mismatch");
    require(name->type().id == LogicalTypeId::Varchar, "varchar type mismatch");
    require(name->type().parameter.value() == 64, "varchar length mismatch");
    require(name->default_expression().has_value(), "default expression missing");
    require(schema.find_column(name->column_id()) == name, "find by column id mismatch");

    require(embedding->type().id == LogicalTypeId::Vector, "vector type mismatch");
    require(embedding->type().parameter.value() == 3, "vector dimension mismatch");
    require(schema.column_at(3) == nullptr, "out-of-range ordinal should return null");
}

void test_load_missing_collection_schema_fails()
{
    CatalogEditor catalog;
    auto loaded = load_collection_schema(catalog.view(), 999);
    require(!loaded.has_value(), "missing collection schema should fail");
    require(loaded.error().code == SchemaLoadErrorCode::CollectionNotFound, "missing collection error code mismatch");
}

} // namespace

int main()
{
    try {
        test_load_collection_schema_from_catalog();
        test_load_missing_collection_schema_fails();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
