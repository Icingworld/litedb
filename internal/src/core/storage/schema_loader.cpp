#include "core/storage/schema_loader.hpp"

#include "core/meta/meta.hpp"

#include <utility>

namespace litedb::core::storage
{

namespace
{

SchemaLoadError make_error(SchemaLoadErrorCode code, std::string message)
{
    return SchemaLoadError {code, std::move(message)};
}

} // namespace

std::expected<schema::CollectionSchema, SchemaLoadError> load_collection_schema(
    const meta::CatalogView & catalog,
    common::CollectionId collection_id
)
{
    const auto * collection = catalog.find_collection(collection_id);
    if (collection == nullptr) {
        return std::unexpected(make_error(SchemaLoadErrorCode::CollectionNotFound, "Collection not found"));
    }

    const auto * database = catalog.find_database(collection->database_id());
    if (database == nullptr) {
        return std::unexpected(make_error(SchemaLoadErrorCode::DatabaseNotFound, "Database not found"));
    }

    const auto catalog_columns = catalog.list_columns(collection_id);
    std::vector<schema::ColumnSchema> columns;
    columns.reserve(catalog_columns.size());

    for (std::size_t ordinal = 0; ordinal < catalog_columns.size(); ++ordinal) {
        const auto * column = catalog_columns[ordinal];
        if (column == nullptr) {
            continue;
        }

        columns.emplace_back(
            column->id(),
            column->collection_id(),
            ordinal,
            column->name(),
            column->type(),
            column->nullable(),
            column->unique(),
            column->default_expression(),
            column->comment()
        );
    }

    return schema::CollectionSchema {
        collection->database_id(),
        collection->id(),
        collection->name(),
        std::move(columns),
        collection->comment(),
    };
}

} // namespace litedb::core::storage
