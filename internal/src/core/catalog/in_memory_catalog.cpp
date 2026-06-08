#include "core/catalog/in_memory_catalog.hpp"

#include <unordered_set>
#include <utility>

namespace litedb::core::catalog
{

namespace
{

/**
 * @brief 创建错误
 * @param code 错误码
 * @param message 错误消息
 * @return 错误
 */
CatalogError make_error(CatalogErrorCode code, std::string message)
{
    return CatalogError {code, std::move(message)};
}

/**
 * @brief 判断字符串是否为空
 * @param value 字符串
 * @return 是否为空
 */
bool blank(std::string_view value)
{
    return value.empty();
}

} // namespace

const DatabaseEntry * InMemoryCatalog::find_database(std::string_view name) const
{
    const auto it = databases_by_key_.find(normalize_identifier(name));
    if (it == databases_by_key_.end()) {
        return nullptr;
    }
    return find_database(it->second);
}

const DatabaseEntry * InMemoryCatalog::find_database(common::DatabaseId database_id) const
{
    const auto it = databases_by_id_.find(database_id);
    if (it == databases_by_id_.end()) {
        return nullptr;
    }
    return it->second.get();
}

DatabaseEntry * InMemoryCatalog::find_database_mutable(common::DatabaseId database_id)
{
    const auto it = databases_by_id_.find(database_id);
    if (it == databases_by_id_.end()) {
        return nullptr;
    }
    return it->second.get();
}

const CollectionEntry * InMemoryCatalog::find_collection(
    common::DatabaseId database_id,
    std::string_view name
) const
{
    const auto * database = find_database(database_id);
    if (database == nullptr) {
        return nullptr;
    }

    const auto collection_id = database->find_collection_id(normalize_identifier(name));
    if (!collection_id.has_value()) {
        return nullptr;
    }
    return find_collection(collection_id.value());
}

const CollectionEntry * InMemoryCatalog::find_collection(common::CollectionId collection_id) const
{
    const auto it = collections_by_id_.find(collection_id);
    if (it == collections_by_id_.end()) {
        return nullptr;
    }
    return it->second.get();
}

const ColumnEntry * InMemoryCatalog::find_column(
    common::CollectionId collection_id,
    std::string_view name
) const
{
    const auto * collection = find_collection(collection_id);
    if (collection == nullptr) {
        return nullptr;
    }

    const auto column_id = collection->find_column_id(normalize_identifier(name));
    if (!column_id.has_value()) {
        return nullptr;
    }
    return find_column(column_id.value());
}

const ColumnEntry * InMemoryCatalog::find_column(common::ColumnId column_id) const
{
    const auto it = columns_by_id_.find(column_id);
    if (it == columns_by_id_.end()) {
        return nullptr;
    }
    return it->second.get();
}

std::vector<const DatabaseEntry *> InMemoryCatalog::list_databases() const
{
    std::vector<const DatabaseEntry *> databases;
    databases.reserve(database_ids_.size());
    for (const auto database_id : database_ids_) {
        if (const auto * database = find_database(database_id); database != nullptr) {
            databases.push_back(database);
        }
    }
    return databases;
}

std::vector<const CollectionEntry *> InMemoryCatalog::list_collections(common::DatabaseId database_id) const
{
    const auto * database = find_database(database_id);
    if (database == nullptr) {
        return {};
    }

    std::vector<const CollectionEntry *> collections;
    collections.reserve(database->collection_ids().size());
    for (const auto collection_id : database->collection_ids()) {
        if (const auto * collection = find_collection(collection_id); collection != nullptr) {
            collections.push_back(collection);
        }
    }
    return collections;
}

std::vector<const ColumnEntry *> InMemoryCatalog::list_columns(common::CollectionId collection_id) const
{
    const auto * collection = find_collection(collection_id);
    if (collection == nullptr) {
        return {};
    }

    std::vector<const ColumnEntry *> columns;
    columns.reserve(collection->column_ids().size());
    for (const auto column_id : collection->column_ids()) {
        if (const auto * column = find_column(column_id); column != nullptr) {
            columns.push_back(column);
        }
    }
    return columns;
}

std::expected<common::DatabaseId, CatalogError> InMemoryCatalog::create_database(
    const CreateDatabaseRequest & request
)
{
    if (blank(request.name)) {
        return std::unexpected(make_error(CatalogErrorCode::InvalidArgument, "Database name cannot be empty"));
    }

    const auto key = normalize_identifier(request.name);
    const auto existing = databases_by_key_.find(key);
    if (existing != databases_by_key_.end()) {
        if (request.if_not_exists) {
            return existing->second;
        }
        return std::unexpected(make_error(CatalogErrorCode::DuplicateDatabase, "Database already exists: " + request.name));
    }

    const auto database_id = next_database_id();
    auto database = std::make_unique<DatabaseEntry>(database_id, request.name);
    databases_by_key_.emplace(database->key(), database_id);
    databases_by_id_.emplace(database_id, std::move(database));
    database_ids_.push_back(database_id);
    return database_id;
}

std::expected<void, CatalogError> InMemoryCatalog::drop_database(const DropDatabaseRequest & request)
{
    if (blank(request.name)) {
        return std::unexpected(make_error(CatalogErrorCode::InvalidArgument, "Database name cannot be empty"));
    }

    const auto key = normalize_identifier(request.name);
    const auto database_it = databases_by_key_.find(key);
    if (database_it == databases_by_key_.end()) {
        if (request.if_exists) {
            return {};
        }
        return std::unexpected(make_error(CatalogErrorCode::DatabaseNotFound, "Database not found: " + request.name));
    }

    const auto database_id = database_it->second;
    const auto * database = find_database(database_id);
    if (database != nullptr) {
        const auto collection_ids = database->collection_ids();
        for (const auto collection_id : collection_ids) {
            const auto * collection = find_collection(collection_id);
            if (collection == nullptr) {
                continue;
            }

            for (const auto column_id : collection->column_ids()) {
                columns_by_id_.erase(column_id);
            }
            collections_by_id_.erase(collection_id);
        }
    }

    databases_by_key_.erase(database_it);
    databases_by_id_.erase(database_id);
    std::erase(database_ids_, database_id);
    return {};
}

std::expected<common::CollectionId, CatalogError> InMemoryCatalog::create_collection(
    const CreateCollectionRequest & request
)
{
    auto validation = validate_collection_request(request);
    if (!validation.has_value()) {
        return std::unexpected(std::move(validation.error()));
    }

    auto * database = find_database_mutable(request.database_id);
    if (database == nullptr) {
        return std::unexpected(make_error(CatalogErrorCode::DatabaseNotFound, "Database not found"));
    }

    const auto collection_key = normalize_identifier(request.name);
    const auto existing = database->find_collection_id(collection_key);
    if (existing.has_value()) {
        if (request.if_not_exists) {
            return existing.value();
        }
        return std::unexpected(make_error(CatalogErrorCode::DuplicateCollection, "Collection already exists: " + request.name));
    }

    const auto collection_id = next_collection_id();
    auto collection = std::make_unique<CollectionEntry>(collection_id, request.database_id, request.name);

    for (const auto & column_request : request.columns) {
        const auto column_id = next_column_id();
        auto column = std::make_unique<ColumnEntry>(
            column_id,
            collection_id,
            column_request.name,
            column_request.type,
            column_request.primary_key,
            column_request.unique,
            column_request.nullable,
            column_request.default_expression,
            column_request.comment
        );
        collection->add_column(column->key(), column_id, column->primary_key());
        columns_by_id_.emplace(column_id, std::move(column));
    }

    database->add_collection(collection->key(), collection_id);
    collections_by_id_.emplace(collection_id, std::move(collection));
    return collection_id;
}

std::expected<void, CatalogError> InMemoryCatalog::drop_collection(const DropCollectionRequest & request)
{
    if (blank(request.name)) {
        return std::unexpected(make_error(CatalogErrorCode::InvalidArgument, "Collection name cannot be empty"));
    }

    auto * database = find_database_mutable(request.database_id);
    if (database == nullptr) {
        return std::unexpected(make_error(CatalogErrorCode::DatabaseNotFound, "Database not found"));
    }

    const auto collection_key = normalize_identifier(request.name);
    const auto collection_id = database->find_collection_id(collection_key);
    if (!collection_id.has_value()) {
        if (request.if_exists) {
            return {};
        }
        return std::unexpected(make_error(CatalogErrorCode::CollectionNotFound, "Collection not found: " + request.name));
    }

    const auto * collection = find_collection(collection_id.value());
    if (collection != nullptr) {
        for (const auto column_id : collection->column_ids()) {
            columns_by_id_.erase(column_id);
        }
    }

    database->remove_collection(collection_key, collection_id.value());
    collections_by_id_.erase(collection_id.value());
    return {};
}

std::expected<void, CatalogError> InMemoryCatalog::validate_collection_request(
    const CreateCollectionRequest & request
) const
{
    if (blank(request.name)) {
        return std::unexpected(make_error(CatalogErrorCode::InvalidArgument, "Collection name cannot be empty"));
    }

    if (request.columns.empty()) {
        return std::unexpected(make_error(CatalogErrorCode::InvalidArgument, "Collection must have at least one column"));
    }

    std::unordered_set<std::string> column_keys;
    bool has_primary_key = false;
    for (const auto & column : request.columns) {
        if (blank(column.name)) {
            return std::unexpected(make_error(CatalogErrorCode::InvalidArgument, "Column name cannot be empty"));
        }

        const auto [_, inserted] = column_keys.emplace(normalize_identifier(column.name));
        if (!inserted) {
            return std::unexpected(make_error(CatalogErrorCode::DuplicateColumn, "Duplicate column: " + column.name));
        }

        if (column.primary_key) {
            if (has_primary_key) {
                return std::unexpected(make_error(CatalogErrorCode::MultiplePrimaryKeys, "Collection cannot have multiple primary keys"));
            }
            has_primary_key = true;
        }
    }

    return {};
}

common::DatabaseId InMemoryCatalog::next_database_id() noexcept
{
    return next_database_id_++;
}

common::CollectionId InMemoryCatalog::next_collection_id() noexcept
{
    return next_collection_id_++;
}

common::ColumnId InMemoryCatalog::next_column_id() noexcept
{
    return next_column_id_++;
}

} // namespace litedb::core::catalog
