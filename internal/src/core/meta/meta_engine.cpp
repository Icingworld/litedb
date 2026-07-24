#include "core/meta/meta_engine.hpp"

#include <algorithm>
#include <limits>
#include <unordered_set>
#include <utility>

#include "core/meta/meta_helper.hpp"

namespace litedb::core::meta
{

namespace
{

/**
 * @brief 判断标识符是否为空
 * @param value 标识符
 * @return 是否为空
 */
[[nodiscard]]
bool blank(std::string_view value) noexcept
{
    return value.empty();
}

[[nodiscard]]
bool valid_logical_type(const common::LogicalType & type) noexcept
{
    if (static_cast<std::uint8_t>(type.id) > static_cast<std::uint8_t>(common::LogicalTypeId::Vector)) {
        return false;
    }
    if (type.id == common::LogicalTypeId::Varchar || type.id == common::LogicalTypeId::Vector) {
        return type.parameter.has_value() && *type.parameter != 0;
    }
    return !type.parameter.has_value();
}

[[nodiscard]]
bool valid_default_expression(const schema::DefaultExpression & expression, std::size_t depth = 0) noexcept
{
    constexpr std::size_t MaximumDepth = 64;
    if (depth >= MaximumDepth
        || static_cast<std::uint8_t>(expression.kind)
               > static_cast<std::uint8_t>(schema::DefaultExpressionKind::Vector)
        || static_cast<std::uint8_t>(expression.literal_kind)
               > static_cast<std::uint8_t>(schema::DefaultLiteralKind::String)) {
        return false;
    }
    if (expression.kind == schema::DefaultExpressionKind::Literal) {
        return expression.elements.empty();
    }
    if (!expression.value.empty() || expression.elements.empty()) {
        return false;
    }
    return std::ranges::all_of(expression.elements, [depth](const auto & element) {
        return element.kind == schema::DefaultExpressionKind::Literal
            && valid_default_expression(element, depth + 1);
    });
}

template <typename Id>
[[nodiscard]]
bool can_allocate(Id next, std::size_t count = 1) noexcept
{
    constexpr auto Maximum = std::numeric_limits<Id>::max();
    return next != 0 && count <= static_cast<std::size_t>(Maximum - next);
}

} // namespace

const entry::DatabaseEntry * CatalogState::find_database(std::string_view name) const
{
    const auto key = database_keys_.find(common::normalize_identifier(name));
    return key == database_keys_.end() ? nullptr : find_database(key->second);
}

const entry::DatabaseEntry * CatalogState::find_database(common::DatabaseId id) const
{
    const auto it = databases_.find(id);
    return it == databases_.end() ? nullptr : it->second.get();
}

entry::DatabaseEntry * CatalogState::find_database_mutable(common::DatabaseId id)
{
    const auto it = databases_.find(id);
    return it == databases_.end() ? nullptr : it->second.get();
}

const entry::CollectionEntry * CatalogState::find_collection(common::DatabaseId database_id, std::string_view name) const
{
    const auto * database = find_database(database_id);
    if (database == nullptr) {
        return nullptr;
    }
    const auto id = database->find_collection_id(common::normalize_identifier(name));
    return id ? find_collection(*id) : nullptr;
}

const entry::CollectionEntry * CatalogState::find_collection(common::CollectionId id) const
{
    const auto it = collections_.find(id);
    return it == collections_.end() ? nullptr : it->second.get();
}

entry::CollectionEntry * CatalogState::find_collection_mutable(common::CollectionId id)
{
    const auto it = collections_.find(id);
    return it == collections_.end() ? nullptr : it->second.get();
}

const entry::ColumnEntry * CatalogState::find_column(common::CollectionId collection_id, std::string_view name) const
{
    const auto * collection = find_collection(collection_id);
    if (collection == nullptr) {
        return nullptr;
    }
    const auto id = collection->find_column_id(common::normalize_identifier(name));
    return id ? find_column(*id) : nullptr;
}

const entry::ColumnEntry * CatalogState::find_column(common::ColumnId id) const
{
    const auto it = columns_.find(id);
    return it == columns_.end() ? nullptr : it->second.get();
}

const entry::IndexEntry * CatalogState::find_index(common::CollectionId collection_id, std::string_view name) const
{
    const auto * collection = find_collection(collection_id);
    if (collection == nullptr) {
        return nullptr;
    }
    const auto id = collection->find_index_id(common::normalize_identifier(name));
    return id ? find_index(*id) : nullptr;
}

const entry::IndexEntry * CatalogState::find_index(common::IndexId id) const
{
    const auto it = indexes_.find(id);
    return it == indexes_.end() ? nullptr : it->second.get();
}

const entry::VectorIndexEntry * CatalogState::find_vector_index(common::CollectionId collection_id, std::string_view name) const
{
    const auto * collection = find_collection(collection_id);
    if (collection == nullptr) {
        return nullptr;
    }
    const auto id = collection->find_vector_index_id(common::normalize_identifier(name));
    return id ? find_vector_index(*id) : nullptr;
}

const entry::VectorIndexEntry * CatalogState::find_vector_index(common::VIndexId id) const
{
    const auto it = vector_indexes_.find(id);
    return it == vector_indexes_.end() ? nullptr : it->second.get();
}

std::vector<const entry::DatabaseEntry *> CatalogState::list_databases() const
{
    std::vector<const entry::DatabaseEntry *> result;
    result.reserve(database_ids_.size());
    for (const auto id : database_ids_) {
        if (const auto * value = find_database(id)) {
            result.push_back(value);
        }
    }
    return result;
}

std::vector<const entry::CollectionEntry *> CatalogState::list_collections(common::DatabaseId database_id) const
{
    std::vector<const entry::CollectionEntry *> result;
    const auto * database = find_database(database_id);
    if (database == nullptr) {
        return result;
    }
    result.reserve(database->collection_ids().size());
    for (const auto id : database->collection_ids()) {
        if (const auto * value = find_collection(id)) {
            result.push_back(value);
        }
    }
    return result;
}

std::vector<const entry::ColumnEntry *> CatalogState::list_columns(common::CollectionId collection_id) const
{
    std::vector<const entry::ColumnEntry *> result;
    const auto * collection = find_collection(collection_id);
    if (collection == nullptr) {
        return result;
    }
    result.reserve(collection->column_ids().size());
    for (const auto id : collection->column_ids()) {
        if (const auto * value = find_column(id)) {
            result.push_back(value);
        }
    }
    return result;
}

std::vector<const entry::IndexEntry *> CatalogState::list_indexes(common::CollectionId collection_id) const
{
    std::vector<const entry::IndexEntry *> result;
    const auto * collection = find_collection(collection_id);
    if (collection == nullptr) {
        return result;
    }
    result.reserve(collection->index_ids().size());
    for (const auto id : collection->index_ids()) {
        if (const auto * value = find_index(id)) {
            result.push_back(value);
        }
    }
    return result;
}

std::vector<const entry::VectorIndexEntry *> CatalogState::list_vector_indexes(common::CollectionId collection_id) const
{
    std::vector<const entry::VectorIndexEntry *> result;
    const auto * collection = find_collection(collection_id);
    if (collection == nullptr) {
        return result;
    }
    result.reserve(collection->vector_index_ids().size());
    for (const auto id : collection->vector_index_ids()) {
        if (const auto * value = find_vector_index(id)) {
            result.push_back(value);
        }
    }
    return result;
}

std::expected<common::DatabaseId, MetaError> CatalogState::create_database(const CreateDatabaseRequest & request)
{
    if (blank(request.name)) {
        return std::unexpected(make_error(MetaErrorCode::InvalidArgument, "Database name cannot be empty"));
    }
    const auto key = common::normalize_identifier(request.name);
    if (const auto it = database_keys_.find(key); it != database_keys_.end()) {
        if (request.if_not_exists) {
            return it->second;
        }
        return std::unexpected(make_error(MetaErrorCode::DuplicateDatabase, "Database already exists: " + request.name));
    }
    if (!can_allocate(next_database_id_)) {
        return std::unexpected(make_error(MetaErrorCode::InvalidState, "Database ID space is exhausted"));
    }
    const auto id = next_database_id_++;
    auto database = std::make_unique<entry::DatabaseEntry>(id, request.name);
    database_keys_.emplace(database->key(), id);
    databases_.emplace(id, std::move(database));
    database_ids_.push_back(id);
    return id;
}

std::expected<void, MetaError> CatalogState::drop_database(const DropDatabaseRequest & request)
{
    if (blank(request.name)) {
        return std::unexpected(make_error(MetaErrorCode::InvalidArgument, "Database name cannot be empty"));
    }
    auto * database = const_cast<entry::DatabaseEntry *>(find_database(request.name));
    if (database == nullptr) {
        if (request.if_exists) {
            return {};
        }
        return std::unexpected(make_error(MetaErrorCode::DatabaseNotFound, "Database not found: " + request.name));
    }
    const auto id = database->id();
    const auto key = database->key();
    const auto children = database->collection_ids();
    for (const auto collection_id : children) {
        erase_collection(collection_id);
    }
    databases_.erase(id);
    database_keys_.erase(key);
    std::erase(database_ids_, id);
    return {};
}

std::expected<common::CollectionId, MetaError> CatalogState::create_collection(const CreateCollectionRequest & request)
{
    auto * database = find_database_mutable(request.database_id);
    if (database == nullptr) {
        return std::unexpected(make_error(MetaErrorCode::DatabaseNotFound, "Database not found"));
    }
    if (blank(request.name)) {
        return std::unexpected(make_error(MetaErrorCode::InvalidArgument, "Collection name cannot be empty"));
    }
    const auto key = common::normalize_identifier(request.name);
    if (const auto existing = database->find_collection_id(key)) {
        if (request.if_not_exists) {
            return *existing;
        }
        return std::unexpected(make_error(MetaErrorCode::DuplicateCollection, "Collection already exists: " + request.name));
    }
    if (request.columns.empty()) {
        return std::unexpected(make_error(MetaErrorCode::InvalidArgument, "Collection must have at least one column"));
    }
    std::unordered_set<std::string> column_keys;
    for (const auto & column : request.columns) {
        if (blank(column.name)) {
            return std::unexpected(make_error(MetaErrorCode::InvalidArgument, "Column name cannot be empty"));
        }
        if (!column_keys.insert(common::normalize_identifier(column.name)).second) {
            return std::unexpected(make_error(MetaErrorCode::DuplicateColumn, "Duplicate column: " + column.name));
        }
        if (!valid_logical_type(column.type)
            || (column.default_expression && !valid_default_expression(*column.default_expression))) {
            return std::unexpected(make_error(
                MetaErrorCode::InvalidArgument,
                "Invalid column type or default expression: " + column.name
            ));
        }
    }
    if (!can_allocate(next_collection_id_) || !can_allocate(next_column_id_, request.columns.size())) {
        return std::unexpected(make_error(
            MetaErrorCode::InvalidState,
            "Collection or column ID space is exhausted"
        ));
    }
    const auto id = next_collection_id_++;
    auto collection = std::make_unique<entry::CollectionEntry>(id, request.database_id, request.name, request.comment);
    for (std::size_t ordinal = 0; ordinal < request.columns.size(); ++ordinal) {
        const auto & definition = request.columns[ordinal];
        const auto column_id = next_column_id_++;
        auto column = std::make_unique<entry::ColumnEntry>(
            column_id,
            id,
            ordinal,
            definition.name,
            definition.type,
            definition.unique,
            definition.nullable,
            definition.default_expression,
            definition.comment
        );
        collection->add_column(column->key(), column_id);
        columns_.emplace(column_id, std::move(column));
    }
    database->add_collection(collection->key(), id);
    collections_.emplace(id, std::move(collection));
    return id;
}

void CatalogState::erase_collection(common::CollectionId id)
{
    auto * collection = find_collection_mutable(id);
    if (collection == nullptr) {
        return;
    }
    for (const auto child : collection->column_ids()) {
        columns_.erase(child);
    }
    for (const auto child : collection->index_ids()) {
        indexes_.erase(child);
    }
    for (const auto child : collection->vector_index_ids()) {
        vector_indexes_.erase(child);
    }
    collections_.erase(id);
}

std::expected<void, MetaError> CatalogState::drop_collection(const DropCollectionRequest & request)
{
    auto * database = find_database_mutable(request.database_id);
    if (database == nullptr) {
        return std::unexpected(make_error(MetaErrorCode::DatabaseNotFound, "Database not found"));
    }
    const auto * collection = find_collection(request.database_id, request.name);
    if (collection == nullptr) {
        if (request.if_exists) {
            return {};
        }
        return std::unexpected(make_error(MetaErrorCode::CollectionNotFound, "Collection not found: " + request.name));
    }
    const auto id = collection->id();
    const auto key = collection->key();
    database->remove_collection(key);
    erase_collection(id);
    return {};
}

std::expected<common::IndexId, MetaError> CatalogState::create_index(const CreateIndexRequest & request)
{
    auto * collection = find_collection_mutable(request.collection_id);
    if (collection == nullptr) {
        return std::unexpected(make_error(MetaErrorCode::CollectionNotFound, "Collection not found"));
    }
    if (blank(request.name) || request.column_ids.empty()) {
        return std::unexpected(make_error(MetaErrorCode::InvalidArgument, "Index name and columns cannot be empty"));
    }
    const auto key = common::normalize_identifier(request.name);
    if (const auto existing = collection->find_index_id(key)) {
        if (request.if_not_exists) {
            return *existing;
        }
        return std::unexpected(make_error(MetaErrorCode::DuplicateIndex, "Index already exists: " + request.name));
    }
    if (collection->contains_vector_index(key)) {
        return std::unexpected(make_error(MetaErrorCode::DuplicateIndex, "Index name already exists: " + request.name));
    }
    if (request.kind != entry::IndexKind::BTree) {
        return std::unexpected(make_error(MetaErrorCode::InvalidArgument, "Scalar index kind must be BTREE"));
    }
    std::unordered_set<common::ColumnId> seen;
    for (const auto column_id : request.column_ids) {
        const auto * column = find_column(column_id);
        if (column == nullptr || column->collection_id() != request.collection_id) {
            return std::unexpected(make_error(MetaErrorCode::ColumnNotFound, "Index column not found"));
        }
        if (column->type().id == common::LogicalTypeId::Vector) {
            return std::unexpected(make_error(MetaErrorCode::InvalidArgument, "Scalar index cannot contain a VECTOR column"));
        }
        if (!seen.insert(column_id).second) {
            return std::unexpected(make_error(MetaErrorCode::InvalidArgument, "Index contains duplicate columns"));
        }
    }
    if (!can_allocate(next_index_id_)) {
        return std::unexpected(make_error(MetaErrorCode::InvalidState, "Index ID space is exhausted"));
    }
    const auto id = next_index_id_++;
    auto index = std::make_unique<entry::IndexEntry>(
        id,
        request.collection_id,
        request.column_ids,
        request.name,
        request.kind,
        request.unique
    );
    collection->add_index(index->key(), id);
    indexes_.emplace(id, std::move(index));
    return id;
}

std::expected<void, MetaError> CatalogState::drop_index(const DropIndexRequest & request)
{
    auto * collection = find_collection_mutable(request.collection_id);
    if (collection == nullptr) {
        return std::unexpected(make_error(MetaErrorCode::CollectionNotFound, "Collection not found"));
    }
    const auto * index = find_index(request.collection_id, request.name);
    if (index == nullptr) {
        if (request.if_exists) {
            return {};
        }
        return std::unexpected(make_error(MetaErrorCode::IndexNotFound, "Index not found: " + request.name));
    }
    const auto id = index->id();
    const auto key = index->key();
    collection->remove_index(key);
    indexes_.erase(id);
    return {};
}

std::expected<common::VIndexId, MetaError> CatalogState::create_vector_index(const CreateVectorIndexRequest & request)
{
    auto * collection = find_collection_mutable(request.collection_id);
    if (collection == nullptr) {
        return std::unexpected(make_error(MetaErrorCode::CollectionNotFound, "Collection not found"));
    }
    if (blank(request.name)) {
        return std::unexpected(make_error(MetaErrorCode::InvalidArgument, "Vector index name cannot be empty"));
    }
    const auto key = common::normalize_identifier(request.name);
    if (const auto existing = collection->find_vector_index_id(key)) {
        if (request.if_not_exists) {
            return *existing;
        }
        return std::unexpected(make_error(MetaErrorCode::DuplicateVectorIndex, "Vector index already exists: " + request.name));
    }
    if (collection->contains_index(key)) {
        return std::unexpected(make_error(MetaErrorCode::DuplicateVectorIndex, "Index name already exists: " + request.name));
    }
    const auto * column = find_column(request.column_id);
    if (column == nullptr || column->collection_id() != request.collection_id) {
        return std::unexpected(make_error(MetaErrorCode::ColumnNotFound, "Vector index column not found"));
    }
    if (column->type().id != common::LogicalTypeId::Vector || !column->type().parameter || *column->type().parameter == 0) {
        return std::unexpected(make_error(MetaErrorCode::InvalidArgument, "Vector index requires a VECTOR(n) column"));
    }
    if (request.kind != entry::VectorIndexKind::Hnsw
        || static_cast<std::uint8_t>(request.metric)
               > static_cast<std::uint8_t>(entry::VectorDistanceMetric::Cosine)) {
        return std::unexpected(make_error(MetaErrorCode::InvalidArgument, "Invalid vector index kind or metric"));
    }
    if (request.hnsw_options.max_neighbors == 0
        || request.hnsw_options.ef_construction < request.hnsw_options.max_neighbors
        || request.hnsw_options.ef_search_default == 0) {
        return std::unexpected(make_error(MetaErrorCode::InvalidArgument, "Invalid HNSW options"));
    }
    if (!can_allocate(next_vector_index_id_)) {
        return std::unexpected(make_error(MetaErrorCode::InvalidState, "Vector index ID space is exhausted"));
    }
    const auto id = next_vector_index_id_++;
    auto index = std::make_unique<entry::VectorIndexEntry>(
        id,
        request.collection_id,
        request.column_id,
        request.name,
        request.kind,
        request.metric,
        *column->type().parameter,
        request.hnsw_options
    );
    collection->add_vector_index(index->key(), id);
    vector_indexes_.emplace(id, std::move(index));
    return id;
}

std::expected<void, MetaError> CatalogState::drop_vector_index(const DropVectorIndexRequest & request)
{
    auto * collection = find_collection_mutable(request.collection_id);
    if (collection == nullptr) {
        return std::unexpected(make_error(MetaErrorCode::CollectionNotFound, "Collection not found"));
    }
    const auto * index = find_vector_index(request.collection_id, request.name);
    if (index == nullptr) {
        if (request.if_exists) {
            return {};
        }
        return std::unexpected(make_error(MetaErrorCode::VectorIndexNotFound, "Vector index not found: " + request.name));
    }
    const auto id = index->id();
    const auto key = index->key();
    collection->remove_vector_index(key);
    vector_indexes_.erase(id);
    return {};
}

MetaSnapshot CatalogState::snapshot() const
{
    MetaSnapshot result;
    result.next_database_id = next_database_id_;
    result.next_collection_id = next_collection_id_;
    result.next_column_id = next_column_id_;
    result.next_index_id = next_index_id_;
    result.next_vector_index_id = next_vector_index_id_;
    for (const auto * database : list_databases()) {
        MetaSnapshotDatabase database_snapshot {database->id(), database->name(), {}};
        for (const auto * collection : list_collections(database->id())) {
            MetaSnapshotCollection collection_snapshot;
            collection_snapshot.id = collection->id();
            collection_snapshot.database_id = database->id();
            collection_snapshot.name = collection->name();
            collection_snapshot.comment = collection->comment();
            for (const auto * column : list_columns(collection->id())) {
                collection_snapshot.columns.push_back({
                    column->id(),
                    column->name(),
                    column->type(),
                    column->unique(),
                    column->nullable(),
                    column->default_expression(),
                    column->comment(),
                });
            }
            for (const auto * index : list_indexes(collection->id())) {
                collection_snapshot.indexes.push_back({
                    index->id(),
                    index->column_ids(),
                    index->name(),
                    index->kind(),
                    index->unique(),
                });
            }
            for (const auto * index : list_vector_indexes(collection->id())) {
                collection_snapshot.vector_indexes.push_back({
                    index->id(),
                    index->column_id(),
                    index->name(),
                    index->index_kind(),
                    index->metric(),
                    index->dimension(),
                    index->max_neighbors(),
                    index->ef_construction(),
                    index->ef_search_default(),
                    index->random_seed(),
                });
            }
            database_snapshot.collections.push_back(std::move(collection_snapshot));
        }
        result.databases.push_back(std::move(database_snapshot));
    }
    return result;
}

std::expected<void, MetaError> CatalogState::restore(const MetaSnapshot & source)
{
    CatalogState rebuilt;
    rebuilt.next_database_id_ = source.next_database_id;
    rebuilt.next_collection_id_ = source.next_collection_id;
    rebuilt.next_column_id_ = source.next_column_id;
    rebuilt.next_index_id_ = source.next_index_id;
    rebuilt.next_vector_index_id_ = source.next_vector_index_id;
    common::DatabaseId max_database = 0;
    common::CollectionId max_collection = 0;
    common::ColumnId max_column = 0;
    common::IndexId max_index = 0;
    common::VIndexId max_vector_index = 0;
    for (const auto & database_snapshot : source.databases) {
        if (database_snapshot.id == 0 || blank(database_snapshot.name)) return std::unexpected(make_error(MetaErrorCode::InvalidSnapshot, "Invalid database in meta snapshot"));
        auto database = std::make_unique<entry::DatabaseEntry>(database_snapshot.id, database_snapshot.name);
        if (rebuilt.databases_.contains(database_snapshot.id) || rebuilt.database_keys_.contains(database->key())) {
            return std::unexpected(make_error(MetaErrorCode::InvalidSnapshot, "Duplicate database in meta snapshot"));
        }
        auto * database_ptr = database.get();
        rebuilt.database_ids_.push_back(database_snapshot.id);
        rebuilt.database_keys_.emplace(database->key(), database_snapshot.id);
        rebuilt.databases_.emplace(database_snapshot.id, std::move(database));
        max_database = std::max(max_database, database_snapshot.id);
        for (const auto & collection_snapshot : database_snapshot.collections) {
            if (collection_snapshot.id == 0 || collection_snapshot.database_id != database_snapshot.id || blank(collection_snapshot.name)) {
                return std::unexpected(make_error(MetaErrorCode::InvalidSnapshot, "Invalid collection in meta snapshot"));
            }
            if (collection_snapshot.columns.empty()) {
                return std::unexpected(make_error(MetaErrorCode::InvalidSnapshot, "Collection snapshot must contain columns"));
            }
            auto collection = std::make_unique<entry::CollectionEntry>(collection_snapshot.id, database_snapshot.id,
                                                                        collection_snapshot.name, collection_snapshot.comment);
            if (rebuilt.collections_.contains(collection_snapshot.id) || database_ptr->contains_collection(collection->key())) {
                return std::unexpected(make_error(MetaErrorCode::InvalidSnapshot, "Duplicate collection in meta snapshot"));
            }
            auto * collection_ptr = collection.get();
            std::unordered_set<common::ColumnId> collection_columns;
            for (std::size_t ordinal = 0; ordinal < collection_snapshot.columns.size(); ++ordinal) {
                const auto & value = collection_snapshot.columns[ordinal];
                if (value.id == 0 || blank(value.name) || rebuilt.columns_.contains(value.id) || collection_ptr->contains_column(common::normalize_identifier(value.name))) {
                    return std::unexpected(make_error(MetaErrorCode::InvalidSnapshot, "Invalid or duplicate column in meta snapshot"));
                }
                if (!valid_logical_type(value.type)
                    || (value.default_expression && !valid_default_expression(*value.default_expression))) {
                    return std::unexpected(make_error(
                        MetaErrorCode::InvalidSnapshot,
                        "Invalid column type or default expression in meta snapshot"
                    ));
                }
                auto column = std::make_unique<entry::ColumnEntry>(value.id, collection_snapshot.id, ordinal, value.name,
                                                                   value.type, value.unique, value.nullable,
                                                                   value.default_expression, value.comment);
                collection_ptr->add_column(column->key(), value.id);
                rebuilt.columns_.emplace(value.id, std::move(column));
                collection_columns.insert(value.id);
                max_column = std::max(max_column, value.id);
            }
            for (const auto & value : collection_snapshot.indexes) {
                if (value.id == 0 || blank(value.name) || value.column_ids.empty() || rebuilt.indexes_.contains(value.id)
                    || collection_ptr->contains_index(common::normalize_identifier(value.name))
                    || value.index_kind != entry::IndexKind::BTree) {
                    return std::unexpected(make_error(MetaErrorCode::InvalidSnapshot, "Invalid or duplicate index in meta snapshot"));
                }
                std::unordered_set<common::ColumnId> index_columns;
                for (const auto column_id : value.column_ids) {
                    const auto * column = rebuilt.find_column(column_id);
                    if (!collection_columns.contains(column_id) || column == nullptr) {
                        return std::unexpected(make_error(MetaErrorCode::InvalidSnapshot, "Index column not found in meta snapshot"));
                    }
                    if (column->type().id == common::LogicalTypeId::Vector || !index_columns.insert(column_id).second) {
                        return std::unexpected(make_error(MetaErrorCode::InvalidSnapshot, "Invalid scalar index columns in meta snapshot"));
                    }
                }
                auto index = std::make_unique<entry::IndexEntry>(value.id, collection_snapshot.id, value.column_ids,
                                                                 value.name, value.index_kind, value.unique);
                collection_ptr->add_index(index->key(), value.id);
                rebuilt.indexes_.emplace(value.id, std::move(index));
                max_index = std::max(max_index, value.id);
            }
            for (const auto & value : collection_snapshot.vector_indexes) {
                if (value.id == 0 || blank(value.name) || rebuilt.vector_indexes_.contains(value.id)
                    || collection_ptr->contains_index(common::normalize_identifier(value.name))
                    || collection_ptr->contains_vector_index(common::normalize_identifier(value.name))
                    || !collection_columns.contains(value.column_id)
                    || static_cast<std::uint8_t>(value.index_kind) > static_cast<std::uint8_t>(entry::VectorIndexKind::Hnsw)
                    || static_cast<std::uint8_t>(value.metric) > static_cast<std::uint8_t>(entry::VectorDistanceMetric::Cosine)) {
                    return std::unexpected(make_error(MetaErrorCode::InvalidSnapshot, "Invalid or duplicate vector index in meta snapshot"));
                }
                const auto * column = rebuilt.find_column(value.column_id);
                if (column == nullptr || column->type().id != common::LogicalTypeId::Vector || !column->type().parameter
                    || *column->type().parameter != value.dimension || value.max_neighbors == 0
                    || value.ef_construction < value.max_neighbors || value.ef_search_default == 0) {
                    return std::unexpected(make_error(MetaErrorCode::InvalidSnapshot, "Invalid vector index options in meta snapshot"));
                }
                entry::HnswOptions options {value.max_neighbors, value.ef_construction, value.ef_search_default, value.random_seed};
                auto index = std::make_unique<entry::VectorIndexEntry>(value.id, collection_snapshot.id, value.column_id,
                                                                       value.name, value.index_kind, value.metric,
                                                                       value.dimension, options);
                collection_ptr->add_vector_index(index->key(), value.id);
                rebuilt.vector_indexes_.emplace(value.id, std::move(index));
                max_vector_index = std::max(max_vector_index, value.id);
            }
            database_ptr->add_collection(collection->key(), collection_snapshot.id);
            rebuilt.collections_.emplace(collection_snapshot.id, std::move(collection));
            max_collection = std::max(max_collection, collection_snapshot.id);
        }
    }
    if (source.next_database_id <= max_database || source.next_collection_id <= max_collection
        || source.next_column_id <= max_column || source.next_index_id <= max_index
        || source.next_vector_index_id <= max_vector_index) {
        return std::unexpected(make_error(MetaErrorCode::InvalidSnapshot, "Meta snapshot next id is behind existing ids"));
    }
    *this = std::move(rebuilt);
    return {};
}

const entry::DatabaseEntry * CatalogView::find_database(std::string_view name) const
{
    return state_->find_database(name);
}

const entry::DatabaseEntry * CatalogView::find_database(common::DatabaseId id) const
{
    return state_->find_database(id);
}

const entry::CollectionEntry * CatalogView::find_collection(
    common::DatabaseId database_id,
    std::string_view name
) const
{
    return state_->find_collection(database_id, name);
}

const entry::CollectionEntry * CatalogView::find_collection(common::CollectionId id) const
{
    return state_->find_collection(id);
}

const entry::ColumnEntry * CatalogView::find_column(
    common::CollectionId collection_id,
    std::string_view name
) const
{
    return state_->find_column(collection_id, name);
}

const entry::ColumnEntry * CatalogView::find_column(common::ColumnId id) const
{
    return state_->find_column(id);
}

const entry::IndexEntry * CatalogView::find_index(
    common::CollectionId collection_id,
    std::string_view name
) const
{
    return state_->find_index(collection_id, name);
}

const entry::IndexEntry * CatalogView::find_index(common::IndexId id) const
{
    return state_->find_index(id);
}

const entry::VectorIndexEntry * CatalogView::find_vector_index(
    common::CollectionId collection_id,
    std::string_view name
) const
{
    return state_->find_vector_index(collection_id, name);
}

const entry::VectorIndexEntry * CatalogView::find_vector_index(common::VIndexId id) const
{
    return state_->find_vector_index(id);
}

std::vector<const entry::DatabaseEntry *> CatalogView::list_databases() const
{
    return state_->list_databases();
}

std::vector<const entry::CollectionEntry *> CatalogView::list_collections(
    common::DatabaseId database_id
) const
{
    return state_->list_collections(database_id);
}

std::vector<const entry::ColumnEntry *> CatalogView::list_columns(
    common::CollectionId collection_id
) const
{
    return state_->list_columns(collection_id);
}

std::vector<const entry::IndexEntry *> CatalogView::list_indexes(
    common::CollectionId collection_id
) const
{
    return state_->list_indexes(collection_id);
}

std::vector<const entry::VectorIndexEntry *> CatalogView::list_vector_indexes(
    common::CollectionId collection_id
) const
{
    return state_->list_vector_indexes(collection_id);
}

MetaSnapshot CatalogView::snapshot() const
{
    return state_->snapshot();
}

std::expected<CatalogState, MetaError> build_catalog_state(const MetaSnapshot & snapshot)
{
    CatalogState state;
    if (auto restored = state.restore(snapshot); !restored) {
        return std::unexpected(std::move(restored.error()));
    }
    return state;
}

std::expected<CatalogEditor, MetaError> CatalogEditor::from(CatalogView source)
{
    return from(source.snapshot());
}

std::expected<CatalogEditor, MetaError> CatalogEditor::from(const MetaSnapshot & source)
{
    auto state = build_catalog_state(source);
    if (!state) {
        return std::unexpected(std::move(state.error()));
    }
    CatalogEditor editor;
    editor.state_ = std::move(*state);
    return editor;
}

std::expected<common::DatabaseId, MetaError> CatalogEditor::create_database(
    const CreateDatabaseRequest & request
)
{
    return state_.create_database(request);
}

std::expected<void, MetaError> CatalogEditor::drop_database(const DropDatabaseRequest & request)
{
    return state_.drop_database(request);
}

std::expected<common::CollectionId, MetaError> CatalogEditor::create_collection(
    const CreateCollectionRequest & request
)
{
    return state_.create_collection(request);
}

std::expected<void, MetaError> CatalogEditor::drop_collection(const DropCollectionRequest & request)
{
    return state_.drop_collection(request);
}

std::expected<common::IndexId, MetaError> CatalogEditor::create_index(
    const CreateIndexRequest & request
)
{
    return state_.create_index(request);
}

std::expected<void, MetaError> CatalogEditor::drop_index(const DropIndexRequest & request)
{
    return state_.drop_index(request);
}

std::expected<common::VIndexId, MetaError> CatalogEditor::create_vector_index(
    const CreateVectorIndexRequest & request
)
{
    return state_.create_vector_index(request);
}

std::expected<void, MetaError> CatalogEditor::drop_vector_index(
    const DropVectorIndexRequest & request
)
{
    return state_.drop_vector_index(request);
}

CatalogPublisher::CatalogPublisher(
    std::filesystem::path path,
    filesystem::FileSystem & filesystem
)
    : store_(std::move(path), filesystem)
{
}

std::expected<void, MetaError> CatalogPublisher::open_or_initialize()
{
    auto loaded = store_.load();
    if (!loaded) {
        return std::unexpected(std::move(loaded.error()));
    }
    MetaSnapshot snapshot;
    if (*loaded) {
        snapshot = std::move(**loaded);
    } else {
        if (auto saved = store_.save(snapshot); !saved) {
            return std::unexpected(std::move(saved.error()));
        }
    }
    return publish_committed(snapshot);
}

std::expected<void, MetaError> CatalogPublisher::publish_committed(const MetaSnapshot & snapshot)
{
    auto rebuilt = build_catalog_state(snapshot);
    if (!rebuilt) {
        return std::unexpected(std::move(rebuilt.error()));
    }
    state_ = std::move(*rebuilt);
    return {};
}

} // namespace litedb::core::meta
