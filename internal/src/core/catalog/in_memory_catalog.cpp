#include "core/catalog/in_memory_catalog.hpp"

#include <algorithm>
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
CatalogError make_catalog_error(CatalogErrorCode code, std::string message)
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

CollectionEntry * InMemoryCatalog::find_collection_mutable(common::CollectionId collection_id)
{
    const auto it = collections_by_id_.find(collection_id);
    if (it == collections_by_id_.end()) {
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

const IndexEntry * InMemoryCatalog::find_index(
    common::CollectionId collection_id,
    std::string_view name
) const
{
    const auto * collection = find_collection(collection_id);
    if (collection == nullptr) {
        return nullptr;
    }

    const auto index_id = collection->find_index_id(normalize_identifier(name));
    if (!index_id.has_value()) {
        return nullptr;
    }
    return find_index(index_id.value());
}

const IndexEntry * InMemoryCatalog::find_index(common::IndexId index_id) const
{
    const auto it = indexes_by_id_.find(index_id);
    if (it == indexes_by_id_.end()) {
        return nullptr;
    }
    return it->second.get();
}

const VectorIndexEntry * InMemoryCatalog::find_vector_index(
    common::CollectionId collection_id,
    std::string_view name
) const
{
    const auto * collection = find_collection(collection_id);
    if (collection == nullptr) {
        return nullptr;
    }

    const auto index_id = collection->find_vector_index_id(normalize_identifier(name));
    if (!index_id.has_value()) {
        return nullptr;
    }
    return find_vector_index(index_id.value());
}

const VectorIndexEntry * InMemoryCatalog::find_vector_index(common::VIndexId index_id) const
{
    const auto it = vector_indexes_by_id_.find(index_id);
    if (it == vector_indexes_by_id_.end()) {
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

std::vector<const IndexEntry *> InMemoryCatalog::list_indexes(common::CollectionId collection_id) const
{
    const auto * collection = find_collection(collection_id);
    if (collection == nullptr) {
        return {};
    }

    std::vector<const IndexEntry *> indexes;
    indexes.reserve(collection->index_ids().size());
    for (const auto index_id : collection->index_ids()) {
        if (const auto * index = find_index(index_id); index != nullptr) {
            indexes.push_back(index);
        }
    }
    return indexes;
}

std::vector<const VectorIndexEntry *> InMemoryCatalog::list_vector_indexes(common::CollectionId collection_id) const
{
    const auto * collection = find_collection(collection_id);
    if (collection == nullptr) {
        return {};
    }

    std::vector<const VectorIndexEntry *> indexes;
    indexes.reserve(collection->vector_index_ids().size());
    for (const auto index_id : collection->vector_index_ids()) {
        if (const auto * index = find_vector_index(index_id); index != nullptr) {
            indexes.push_back(index);
        }
    }
    return indexes;
}

std::expected<common::DatabaseId, CatalogError> InMemoryCatalog::create_database(
    const CreateDatabaseRequest & request
)
{
    if (blank(request.name)) {
        return std::unexpected(make_catalog_error(CatalogErrorCode::InvalidArgument, "Database name cannot be empty"));
    }

    const auto key = normalize_identifier(request.name);
    const auto existing = databases_by_key_.find(key);
    if (existing != databases_by_key_.end()) {
        if (request.if_not_exists) {
            return existing->second;
        }
        return std::unexpected(make_catalog_error(CatalogErrorCode::DuplicateDatabase, "Database already exists: " + request.name));
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
        return std::unexpected(make_catalog_error(CatalogErrorCode::InvalidArgument, "Database name cannot be empty"));
    }

    const auto key = normalize_identifier(request.name);
    const auto database_it = databases_by_key_.find(key);
    if (database_it == databases_by_key_.end()) {
        if (request.if_exists) {
            return {};
        }
        return std::unexpected(make_catalog_error(CatalogErrorCode::DatabaseNotFound, "Database not found: " + request.name));
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
            for (const auto index_id : collection->index_ids()) {
                indexes_by_id_.erase(index_id);
            }
            for (const auto index_id : collection->vector_index_ids()) {
                vector_indexes_by_id_.erase(index_id);
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
        return std::unexpected(make_catalog_error(CatalogErrorCode::DatabaseNotFound, "Database not found"));
    }

    const auto collection_key = normalize_identifier(request.name);
    const auto existing = database->find_collection_id(collection_key);
    if (existing.has_value()) {
        if (request.if_not_exists) {
            return existing.value();
        }
        return std::unexpected(make_catalog_error(CatalogErrorCode::DuplicateCollection, "Collection already exists: " + request.name));
    }

    const auto collection_id = next_collection_id();
    auto collection = std::make_unique<CollectionEntry>(collection_id, request.database_id, request.name, request.comment);

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
        return std::unexpected(make_catalog_error(CatalogErrorCode::InvalidArgument, "Collection name cannot be empty"));
    }

    auto * database = find_database_mutable(request.database_id);
    if (database == nullptr) {
        return std::unexpected(make_catalog_error(CatalogErrorCode::DatabaseNotFound, "Database not found"));
    }

    const auto collection_key = normalize_identifier(request.name);
    const auto collection_id = database->find_collection_id(collection_key);
    if (!collection_id.has_value()) {
        if (request.if_exists) {
            return {};
        }
        return std::unexpected(make_catalog_error(CatalogErrorCode::CollectionNotFound, "Collection not found: " + request.name));
    }

    const auto * collection = find_collection(collection_id.value());
    if (collection != nullptr) {
        for (const auto index_id : collection->index_ids()) {
            indexes_by_id_.erase(index_id);
        }
        for (const auto index_id : collection->vector_index_ids()) {
            vector_indexes_by_id_.erase(index_id);
        }
        for (const auto column_id : collection->column_ids()) {
            columns_by_id_.erase(column_id);
        }
    }

    database->remove_collection(collection_key, collection_id.value());
    collections_by_id_.erase(collection_id.value());
    return {};
}

std::expected<common::IndexId, CatalogError> InMemoryCatalog::create_index(
    const CreateIndexRequest & request
)
{
    auto validation = validate_index_request(request);
    if (!validation.has_value()) {
        return std::unexpected(std::move(validation.error()));
    }

    auto * collection = find_collection_mutable(request.collection_id);
    if (collection == nullptr) {
        return std::unexpected(make_catalog_error(CatalogErrorCode::CollectionNotFound, "Collection not found"));
    }

    const auto index_key = normalize_identifier(request.name);
    const auto existing = collection->find_index_id(index_key);
    if (existing.has_value()) {
        if (request.if_not_exists) {
            return existing.value();
        }
        return std::unexpected(make_catalog_error(CatalogErrorCode::DuplicateIndex, "Index already exists: " + request.name));
    }
    if (collection->find_vector_index_id(index_key).has_value()) {
        return std::unexpected(make_catalog_error(CatalogErrorCode::DuplicateIndex, "Vector index already exists: " + request.name));
    }

    const auto index_id = next_index_id();
    auto index = std::make_unique<IndexEntry>(
        index_id,
        request.collection_id,
        request.column_id,
        request.name,
        request.index_kind,
        request.unique
    );
    collection->add_index(index->key(), index_id);
    indexes_by_id_.emplace(index_id, std::move(index));
    return index_id;
}

std::expected<void, CatalogError> InMemoryCatalog::drop_index(const DropIndexRequest & request)
{
    if (blank(request.name)) {
        return std::unexpected(make_catalog_error(CatalogErrorCode::InvalidArgument, "Index name cannot be empty"));
    }

    auto * collection = find_collection_mutable(request.collection_id);
    if (collection == nullptr) {
        return std::unexpected(make_catalog_error(CatalogErrorCode::CollectionNotFound, "Collection not found"));
    }

    const auto index_key = normalize_identifier(request.name);
    const auto index_id = collection->find_index_id(index_key);
    if (!index_id.has_value()) {
        if (request.if_exists) {
            return {};
        }
        return std::unexpected(make_catalog_error(CatalogErrorCode::IndexNotFound, "Index not found: " + request.name));
    }

    collection->remove_index(index_key, index_id.value());
    indexes_by_id_.erase(index_id.value());
    return {};
}

std::expected<common::VIndexId, CatalogError> InMemoryCatalog::create_vector_index(
    const CreateVectorIndexRequest & request
)
{
    auto validation = validate_vector_index_request(request);
    if (!validation.has_value()) {
        return std::unexpected(std::move(validation.error()));
    }

    auto * collection = find_collection_mutable(request.collection_id);
    if (collection == nullptr) {
        return std::unexpected(make_catalog_error(CatalogErrorCode::CollectionNotFound, "Collection not found"));
    }

    const auto index_key = normalize_identifier(request.name);
    const auto existing = collection->find_vector_index_id(index_key);
    if (existing.has_value()) {
        if (request.if_not_exists) {
            return existing.value();
        }
        return std::unexpected(make_catalog_error(CatalogErrorCode::DuplicateIndex, "Vector index already exists: " + request.name));
    }
    if (collection->find_index_id(index_key).has_value()) {
        return std::unexpected(make_catalog_error(CatalogErrorCode::DuplicateIndex, "Index already exists: " + request.name));
    }

    const auto * column = find_column(request.column_id);
    const auto dimension = column->type().parameter.value();
    const auto index_id = next_vector_index_id();
    auto index = std::make_unique<VectorIndexEntry>(
        index_id,
        request.collection_id,
        request.column_id,
        request.name,
        request.index_kind,
        request.metric,
        dimension,
        request.max_neighbors,
        request.ef_construction,
        request.ef_search_default,
        request.random_seed
    );
    collection->add_vector_index(index->key(), index_id);
    vector_indexes_by_id_.emplace(index_id, std::move(index));
    return index_id;
}

std::expected<void, CatalogError> InMemoryCatalog::drop_vector_index(const DropVectorIndexRequest & request)
{
    if (blank(request.name)) {
        return std::unexpected(make_catalog_error(CatalogErrorCode::InvalidArgument, "Vector index name cannot be empty"));
    }

    auto * collection = find_collection_mutable(request.collection_id);
    if (collection == nullptr) {
        return std::unexpected(make_catalog_error(CatalogErrorCode::CollectionNotFound, "Collection not found"));
    }

    const auto index_key = normalize_identifier(request.name);
    const auto index_id = collection->find_vector_index_id(index_key);
    if (!index_id.has_value()) {
        if (request.if_exists) {
            return {};
        }
        return std::unexpected(make_catalog_error(CatalogErrorCode::IndexNotFound, "Vector index not found: " + request.name));
    }

    collection->remove_vector_index(index_key, index_id.value());
    vector_indexes_by_id_.erase(index_id.value());
    return {};
}

CatalogSnapshot InMemoryCatalog::snapshot() const
{
    CatalogSnapshot result {
        .next_database_id = next_database_id_,
        .next_collection_id = next_collection_id_,
        .next_column_id = next_column_id_,
        .next_index_id = next_index_id_,
        .next_vector_index_id = next_vector_index_id_,
        .databases = {},
    };

    for (const auto database_id : database_ids_) {
        const auto * database = find_database(database_id);
        if (database == nullptr) {
            continue;
        }

        CatalogSnapshotDatabase database_snapshot {
            .id = database->id(),
            .name = database->name(),
            .collections = {},
        };

        for (const auto collection_id : database->collection_ids()) {
            const auto * collection = find_collection(collection_id);
            if (collection == nullptr) {
                continue;
            }

            CatalogSnapshotCollection collection_snapshot {
                .id = collection->id(),
                .database_id = collection->database_id(),
                .name = collection->name(),
                .comment = collection->comment(),
                .columns = {},
            };

            for (const auto column_id : collection->column_ids()) {
                const auto * column = find_column(column_id);
                if (column == nullptr) {
                    continue;
                }

                collection_snapshot.columns.push_back(CatalogSnapshotColumn {
                    .id = column->id(),
                    .name = column->name(),
                    .type = column->type(),
                    .primary_key = column->primary_key(),
                    .unique = column->unique(),
                    .nullable = column->nullable(),
                    .default_expression = column->default_expression(),
                    .comment = column->comment(),
                });
            }

            for (const auto index_id : collection->index_ids()) {
                const auto * index = find_index(index_id);
                if (index == nullptr) {
                    continue;
                }

                collection_snapshot.indexes.push_back(CatalogSnapshotIndex {
                    .id = index->id(),
                    .column_id = index->column_id(),
                    .name = index->name(),
                    .index_kind = index->index_kind(),
                    .unique = index->unique(),
                });
            }

            for (const auto index_id : collection->vector_index_ids()) {
                const auto * index = find_vector_index(index_id);
                if (index == nullptr) {
                    continue;
                }

                collection_snapshot.vector_indexes.push_back(CatalogSnapshotVectorIndex {
                    .id = index->id(),
                    .column_id = index->column_id(),
                    .name = index->name(),
                    .index_kind = index->index_kind(),
                    .metric = index->metric(),
                    .dimension = index->dimension(),
                    .max_neighbors = index->max_neighbors(),
                    .ef_construction = index->ef_construction(),
                    .ef_search_default = index->ef_search_default(),
                    .random_seed = index->random_seed(),
                });
            }

            database_snapshot.collections.push_back(std::move(collection_snapshot));
        }

        result.databases.push_back(std::move(database_snapshot));
    }

    return result;
}

std::expected<void, CatalogError> InMemoryCatalog::restore(const CatalogSnapshot & snapshot)
{
    std::unordered_set<common::DatabaseId> database_ids;
    std::unordered_set<common::CollectionId> collection_ids;
    std::unordered_set<common::ColumnId> column_ids;
    std::unordered_set<common::IndexId> index_ids;
    std::unordered_set<common::VIndexId> vector_index_ids;
    std::unordered_set<std::string> database_keys;

    common::DatabaseId max_database_id = 0;
    common::CollectionId max_collection_id = 0;
    common::ColumnId max_column_id = 0;
    common::IndexId max_index_id = 0;
    common::VIndexId max_vector_index_id = 0;

    for (const auto & database_snapshot : snapshot.databases) {
        if (database_snapshot.id == 0 || blank(database_snapshot.name)) {
            return std::unexpected(make_catalog_error(CatalogErrorCode::InvalidArgument, "Invalid database in catalog snapshot"));
        }

        if (!database_ids.insert(database_snapshot.id).second) {
            return std::unexpected(make_catalog_error(CatalogErrorCode::InvalidArgument, "Duplicate database id in catalog snapshot"));
        }
        if (!database_keys.insert(normalize_identifier(database_snapshot.name)).second) {
            return std::unexpected(make_catalog_error(CatalogErrorCode::DuplicateDatabase, "Duplicate database name in catalog snapshot"));
        }
        max_database_id = std::max(max_database_id, database_snapshot.id);

        std::unordered_set<std::string> collection_keys;
        for (const auto & collection_snapshot : database_snapshot.collections) {
            if (collection_snapshot.id == 0 || collection_snapshot.database_id != database_snapshot.id || blank(collection_snapshot.name)) {
                return std::unexpected(make_catalog_error(CatalogErrorCode::InvalidArgument, "Invalid collection in catalog snapshot"));
            }
            if (!collection_ids.insert(collection_snapshot.id).second) {
                return std::unexpected(make_catalog_error(CatalogErrorCode::InvalidArgument, "Duplicate collection id in catalog snapshot"));
            }
            if (!collection_keys.insert(normalize_identifier(collection_snapshot.name)).second) {
                return std::unexpected(make_catalog_error(CatalogErrorCode::DuplicateCollection, "Duplicate collection name in catalog snapshot"));
            }
            if (collection_snapshot.columns.empty()) {
                return std::unexpected(make_catalog_error(CatalogErrorCode::InvalidArgument, "Collection snapshot must contain columns"));
            }
            max_collection_id = std::max(max_collection_id, collection_snapshot.id);

            std::unordered_set<std::string> column_keys;
            std::unordered_set<std::string> index_keys;
            std::unordered_set<std::string> vector_index_keys;
            std::unordered_set<common::ColumnId> collection_column_ids;
            std::unordered_map<common::ColumnId, common::LogicalType> collection_column_types;
            bool has_primary_key = false;
            for (const auto & column_snapshot : collection_snapshot.columns) {
                if (column_snapshot.id == 0 || blank(column_snapshot.name)) {
                    return std::unexpected(make_catalog_error(CatalogErrorCode::InvalidArgument, "Invalid column in catalog snapshot"));
                }
                if (!column_ids.insert(column_snapshot.id).second) {
                    return std::unexpected(make_catalog_error(CatalogErrorCode::InvalidArgument, "Duplicate column id in catalog snapshot"));
                }
                if (!column_keys.insert(normalize_identifier(column_snapshot.name)).second) {
                    return std::unexpected(make_catalog_error(CatalogErrorCode::DuplicateColumn, "Duplicate column name in catalog snapshot"));
                }
                collection_column_ids.insert(column_snapshot.id);
                collection_column_types.emplace(column_snapshot.id, column_snapshot.type);
                if (column_snapshot.primary_key) {
                    if (has_primary_key) {
                        return std::unexpected(make_catalog_error(CatalogErrorCode::MultiplePrimaryKeys, "Multiple primary keys in catalog snapshot"));
                    }
                    has_primary_key = true;
                }
                max_column_id = std::max(max_column_id, column_snapshot.id);
            }

            for (const auto & index_snapshot : collection_snapshot.indexes) {
                if (index_snapshot.id == 0 || blank(index_snapshot.name)) {
                    return std::unexpected(make_catalog_error(CatalogErrorCode::InvalidArgument, "Invalid index in catalog snapshot"));
                }
                if (!index_ids.insert(index_snapshot.id).second) {
                    return std::unexpected(make_catalog_error(CatalogErrorCode::InvalidArgument, "Duplicate index id in catalog snapshot"));
                }
                if (!index_keys.insert(normalize_identifier(index_snapshot.name)).second) {
                    return std::unexpected(make_catalog_error(CatalogErrorCode::DuplicateIndex, "Duplicate index name in catalog snapshot"));
                }
                if (!collection_column_ids.contains(index_snapshot.column_id)) {
                    return std::unexpected(make_catalog_error(CatalogErrorCode::ColumnNotFound, "Index column not found in catalog snapshot"));
                }
                max_index_id = std::max(max_index_id, index_snapshot.id);
            }

            for (const auto & index_snapshot : collection_snapshot.vector_indexes) {
                if (index_snapshot.id == 0 || blank(index_snapshot.name)) {
                    return std::unexpected(make_catalog_error(CatalogErrorCode::InvalidArgument, "Invalid vector index in catalog snapshot"));
                }
                if (!vector_index_ids.insert(index_snapshot.id).second) {
                    return std::unexpected(make_catalog_error(CatalogErrorCode::InvalidArgument, "Duplicate vector index id in catalog snapshot"));
                }
                const auto index_key = normalize_identifier(index_snapshot.name);
                if (index_keys.contains(index_key) || !vector_index_keys.insert(index_key).second) {
                    return std::unexpected(make_catalog_error(CatalogErrorCode::DuplicateIndex, "Duplicate vector index name in catalog snapshot"));
                }
                if (!collection_column_ids.contains(index_snapshot.column_id)) {
                    return std::unexpected(make_catalog_error(CatalogErrorCode::ColumnNotFound, "Vector index column not found in catalog snapshot"));
                }
                const auto column_type = collection_column_types.find(index_snapshot.column_id);
                if (column_type == collection_column_types.end()
                    || column_type->second.id != common::LogicalTypeId::Vector
                    || !column_type->second.parameter.has_value()
                    || column_type->second.parameter.value() != index_snapshot.dimension) {
                    return std::unexpected(make_catalog_error(CatalogErrorCode::InvalidArgument, "Vector index dimension does not match column type in catalog snapshot"));
                }
                if (index_snapshot.dimension == 0
                    || index_snapshot.max_neighbors == 0
                    || index_snapshot.ef_construction < index_snapshot.max_neighbors
                    || index_snapshot.ef_search_default == 0) {
                    return std::unexpected(make_catalog_error(CatalogErrorCode::InvalidArgument, "Invalid vector index options in catalog snapshot"));
                }
                max_vector_index_id = std::max(max_vector_index_id, index_snapshot.id);
            }
        }
    }

    if (snapshot.next_database_id <= max_database_id
        || snapshot.next_collection_id <= max_collection_id
        || snapshot.next_column_id <= max_column_id
        || snapshot.next_index_id <= max_index_id
        || snapshot.next_vector_index_id <= max_vector_index_id) {
        return std::unexpected(make_catalog_error(CatalogErrorCode::InvalidArgument, "Catalog snapshot next id is behind existing ids"));
    }

    next_database_id_ = snapshot.next_database_id;
    next_collection_id_ = snapshot.next_collection_id;
    next_column_id_ = snapshot.next_column_id;
    next_index_id_ = snapshot.next_index_id;
    next_vector_index_id_ = snapshot.next_vector_index_id;
    database_ids_.clear();
    databases_by_id_.clear();
    databases_by_key_.clear();
    collections_by_id_.clear();
    columns_by_id_.clear();
    indexes_by_id_.clear();
    vector_indexes_by_id_.clear();

    for (const auto & database_snapshot : snapshot.databases) {
        auto database = std::make_unique<DatabaseEntry>(database_snapshot.id, database_snapshot.name);
        auto * database_ptr = database.get();
        databases_by_key_.emplace(database->key(), database_snapshot.id);
        databases_by_id_.emplace(database_snapshot.id, std::move(database));
        database_ids_.push_back(database_snapshot.id);

        for (const auto & collection_snapshot : database_snapshot.collections) {
            auto collection = std::make_unique<CollectionEntry>(
                collection_snapshot.id,
                database_snapshot.id,
                collection_snapshot.name,
                collection_snapshot.comment
            );
            auto * collection_ptr = collection.get();

            for (const auto & column_snapshot : collection_snapshot.columns) {
                auto column = std::make_unique<ColumnEntry>(
                    column_snapshot.id,
                    collection_snapshot.id,
                    column_snapshot.name,
                    column_snapshot.type,
                    column_snapshot.primary_key,
                    column_snapshot.unique,
                    column_snapshot.nullable,
                    column_snapshot.default_expression,
                    column_snapshot.comment
                );
                collection_ptr->add_column(column->key(), column_snapshot.id, column->primary_key());
                columns_by_id_.emplace(column_snapshot.id, std::move(column));
            }

            for (const auto & index_snapshot : collection_snapshot.indexes) {
                auto index = std::make_unique<IndexEntry>(
                    index_snapshot.id,
                    collection_snapshot.id,
                    index_snapshot.column_id,
                    index_snapshot.name,
                    index_snapshot.index_kind,
                    index_snapshot.unique
                );
                collection_ptr->add_index(index->key(), index_snapshot.id);
                indexes_by_id_.emplace(index_snapshot.id, std::move(index));
            }

            for (const auto & index_snapshot : collection_snapshot.vector_indexes) {
                auto index = std::make_unique<VectorIndexEntry>(
                    index_snapshot.id,
                    collection_snapshot.id,
                    index_snapshot.column_id,
                    index_snapshot.name,
                    index_snapshot.index_kind,
                    index_snapshot.metric,
                    index_snapshot.dimension,
                    index_snapshot.max_neighbors,
                    index_snapshot.ef_construction,
                    index_snapshot.ef_search_default,
                    index_snapshot.random_seed
                );
                collection_ptr->add_vector_index(index->key(), index_snapshot.id);
                vector_indexes_by_id_.emplace(index_snapshot.id, std::move(index));
            }

            database_ptr->add_collection(collection->key(), collection_snapshot.id);
            collections_by_id_.emplace(collection_snapshot.id, std::move(collection));
        }
    }

    return {};
}

std::expected<void, CatalogError> InMemoryCatalog::validate_index_request(
    const CreateIndexRequest & request
) const
{
    if (blank(request.name)) {
        return std::unexpected(make_catalog_error(CatalogErrorCode::InvalidArgument, "Index name cannot be empty"));
    }

    const auto * collection = find_collection(request.collection_id);
    if (collection == nullptr) {
        return std::unexpected(make_catalog_error(CatalogErrorCode::CollectionNotFound, "Collection not found"));
    }

    const auto * column = find_column(request.column_id);
    if (column == nullptr || column->collection_id() != request.collection_id) {
        return std::unexpected(make_catalog_error(CatalogErrorCode::ColumnNotFound, "Index column not found"));
    }

    if (column->type().id == common::LogicalTypeId::Vector) {
        return std::unexpected(make_catalog_error(CatalogErrorCode::InvalidArgument, "Scalar index cannot be created on VECTOR column"));
    }

    return {};
}

std::expected<void, CatalogError> InMemoryCatalog::validate_vector_index_request(
    const CreateVectorIndexRequest & request
) const
{
    if (blank(request.name)) {
        return std::unexpected(make_catalog_error(CatalogErrorCode::InvalidArgument, "Vector index name cannot be empty"));
    }

    const auto * collection = find_collection(request.collection_id);
    if (collection == nullptr) {
        return std::unexpected(make_catalog_error(CatalogErrorCode::CollectionNotFound, "Collection not found"));
    }

    const auto * column = find_column(request.column_id);
    if (column == nullptr || column->collection_id() != request.collection_id) {
        return std::unexpected(make_catalog_error(CatalogErrorCode::ColumnNotFound, "Vector index column not found"));
    }

    if (column->type().id != common::LogicalTypeId::Vector || !column->type().parameter.has_value() || column->type().parameter.value() == 0) {
        return std::unexpected(make_catalog_error(CatalogErrorCode::InvalidArgument, "Vector index can only be created on VECTOR(n) column"));
    }

    if (request.max_neighbors == 0) {
        return std::unexpected(make_catalog_error(CatalogErrorCode::InvalidArgument, "HNSW max_neighbors must be greater than 0"));
    }

    if (request.ef_construction < request.max_neighbors) {
        return std::unexpected(make_catalog_error(CatalogErrorCode::InvalidArgument, "HNSW ef_construction must be greater than or equal to max_neighbors"));
    }

    if (request.ef_search_default == 0) {
        return std::unexpected(make_catalog_error(CatalogErrorCode::InvalidArgument, "HNSW ef_search_default must be greater than 0"));
    }

    return {};
}

std::expected<void, CatalogError> InMemoryCatalog::validate_collection_request(
    const CreateCollectionRequest & request
) const
{
    if (blank(request.name)) {
        return std::unexpected(make_catalog_error(CatalogErrorCode::InvalidArgument, "Collection name cannot be empty"));
    }

    if (request.columns.empty()) {
        return std::unexpected(make_catalog_error(CatalogErrorCode::InvalidArgument, "Collection must have at least one column"));
    }

    std::unordered_set<std::string> column_keys;
    bool has_primary_key = false;
    for (const auto & column : request.columns) {
        if (blank(column.name)) {
            return std::unexpected(make_catalog_error(CatalogErrorCode::InvalidArgument, "Column name cannot be empty"));
        }

        const auto [_, inserted] = column_keys.emplace(normalize_identifier(column.name));
        if (!inserted) {
            return std::unexpected(make_catalog_error(CatalogErrorCode::DuplicateColumn, "Duplicate column: " + column.name));
        }

        if (column.primary_key) {
            if (has_primary_key) {
                return std::unexpected(make_catalog_error(CatalogErrorCode::MultiplePrimaryKeys, "Collection cannot have multiple primary keys"));
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

common::IndexId InMemoryCatalog::next_index_id() noexcept
{
    return next_index_id_++;
}

common::VIndexId InMemoryCatalog::next_vector_index_id() noexcept
{
    return next_vector_index_id_++;
}

} // namespace litedb::core::catalog
