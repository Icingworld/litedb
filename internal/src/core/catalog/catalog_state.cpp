#include "core/catalog/catalog_state.hpp"

#include <algorithm>
#include <cassert>
#include <unordered_set>
#include <utility>

#include "core/common/identifier.hpp"

namespace litedb::core::catalog
{

namespace
{

[[nodiscard]]
bool empty(std::string_view value) noexcept
{
    return value.empty();
}

[[nodiscard]]
bool valid_logical_type(const common::LogicalType & type) noexcept
{
    if (static_cast<std::uint8_t>(type.id) >
        static_cast<std::uint8_t>(common::LogicalTypeId::Vector)) {
        return false;
    }
    if (type.id == common::LogicalTypeId::Varchar || type.id == common::LogicalTypeId::Vector) {
        return type.parameter.has_value() && *type.parameter != 0;
    }
    return !type.parameter.has_value();
}

[[nodiscard]]
bool valid_default_expression(
    const schema::DefaultExpression & expression,
    std::size_t depth = 0
) noexcept
{
    constexpr std::size_t MaximumDepth = 64;
    if (depth >= MaximumDepth ||
        static_cast<std::uint8_t>(expression.kind) >
            static_cast<std::uint8_t>(schema::DefaultExpressionKind::Vector) ||
        static_cast<std::uint8_t>(expression.literal_kind) >
            static_cast<std::uint8_t>(schema::DefaultLiteralKind::String)) {
        return false;
    }
    if (expression.kind == schema::DefaultExpressionKind::Literal) {
        return expression.elements.empty();
    }
    if (!expression.value.empty() || expression.elements.empty()) {
        return false;
    }
    return std::ranges::all_of(expression.elements, [depth](const auto & element) {
        return element.kind == schema::DefaultExpressionKind::Literal &&
               valid_default_expression(element, depth + 1);
    });
}

template <typename Id>
[[nodiscard]]
bool can_allocate(Id next, std::size_t count = 1) noexcept
{
    constexpr auto Maximum = std::numeric_limits<Id>::max();
    return next != 0 && count <= static_cast<std::size_t>(Maximum - next);
}

[[nodiscard]]
std::string implicit_unique_index_name(common::ColumnId column_id)
{
    return "__litedb_unique_" + std::to_string(column_id);
}

} // namespace

std::optional<const entry::DatabaseEntry &> CatalogState::find_database(std::string_view name) const
{
    const auto it = database_keys_.find(common::normalize_identifier(name));
    if (it == database_keys_.end()) {
        return std::nullopt;
    }
    return find_database(it->second);
}

std::optional<const entry::DatabaseEntry &> CatalogState::find_database(common::DatabaseId id) const
{
    const auto it = databases_.find(id);
    if (it == databases_.end()) {
        return std::nullopt;
    }
    return *it->second.get();
}

std::optional<entry::DatabaseEntry &> CatalogState::find_database_mutable(common::DatabaseId id)
{
    const auto it = databases_.find(id);
    if (it == databases_.end()) {
        return std::nullopt;
    }
    return *it->second.get();
}

std::optional<const entry::CollectionEntry &>
CatalogState::find_collection(common::DatabaseId database_id, std::string_view name) const
{
    const auto database = find_database(database_id);
    if (!database) {
        return std::nullopt;
    }
    const auto id = database->find_collection_id(common::normalize_identifier(name));
    if (!id) {
        return std::nullopt;
    }
    return find_collection(*id);
}

std::optional<const entry::CollectionEntry &> CatalogState::find_collection(
    common::CollectionId id
) const
{
    const auto it = collections_.find(id);
    if (it == collections_.end()) {
        return std::nullopt;
    }
    return *it->second.get();
}

std::optional<entry::CollectionEntry &> CatalogState::find_collection_mutable(
    common::CollectionId id
)
{
    const auto it = collections_.find(id);
    if (it == collections_.end()) {
        return std::nullopt;
    }
    return *it->second.get();
}

std::optional<const entry::ColumnEntry &>
CatalogState::find_column(common::CollectionId collection_id, std::string_view name) const
{
    const auto collection = find_collection(collection_id);
    if (!collection) {
        return std::nullopt;
    }
    const auto id = collection->find_column_id(common::normalize_identifier(name));
    if (!id) {
        return std::nullopt;
    }
    return find_column(*id);
}

std::optional<const entry::ColumnEntry &> CatalogState::find_column(common::ColumnId id) const
{
    const auto it = columns_.find(id);
    if (it == columns_.end()) {
        return std::nullopt;
    }
    return *it->second.get();
}

std::optional<const entry::IndexEntry &>
CatalogState::find_index(common::CollectionId collection_id, std::string_view name) const
{
    const auto collection = find_collection(collection_id);
    if (!collection) {
        return std::nullopt;
    }
    const auto id = collection->find_index_id(common::normalize_identifier(name));
    if (!id) {
        return std::nullopt;
    }
    return find_index(*id);
}

std::optional<const entry::IndexEntry &> CatalogState::find_index(common::IndexId id) const
{
    const auto it = indexes_.find(id);
    if (it == indexes_.end()) {
        return std::nullopt;
    }
    return *it->second.get();
}

std::optional<const entry::VectorIndexEntry &>
CatalogState::find_vector_index(common::CollectionId collection_id, std::string_view name) const
{
    const auto collection = find_collection(collection_id);
    if (!collection) {
        return std::nullopt;
    }
    const auto id = collection->find_vector_index_id(common::normalize_identifier(name));
    if (!id) {
        return std::nullopt;
    }
    return find_vector_index(*id);
}

std::optional<const entry::VectorIndexEntry &> CatalogState::find_vector_index(
    common::VIndexId id
) const
{
    const auto it = vector_indexes_.find(id);
    if (it == vector_indexes_.end()) {
        return std::nullopt;
    }
    return *it->second.get();
}

std::vector<std::reference_wrapper<const entry::DatabaseEntry>> CatalogState::list_databases() const
{
    std::vector<std::reference_wrapper<const entry::DatabaseEntry>> result;
    result.reserve(database_ids_.size());
    for (const auto id : database_ids_) {
        if (const auto value = find_database(id)) {
            result.push_back(*value);
        }
    }
    return result;
}

std::vector<std::reference_wrapper<const entry::CollectionEntry>> CatalogState::list_collections(
    common::DatabaseId database_id
) const
{
    std::vector<std::reference_wrapper<const entry::CollectionEntry>> result;
    const auto database = find_database(database_id);
    if (!database) {
        return result;
    }
    result.reserve(database->collection_ids().size());
    for (const auto id : database->collection_ids()) {
        if (const auto value = find_collection(id)) {
            result.push_back(*value);
        }
    }
    return result;
}

std::vector<std::reference_wrapper<const entry::ColumnEntry>> CatalogState::list_columns(
    common::CollectionId collection_id
) const
{
    std::vector<std::reference_wrapper<const entry::ColumnEntry>> result;
    const auto collection = find_collection(collection_id);
    if (!collection) {
        return result;
    }
    result.reserve(collection->column_ids().size());
    for (const auto id : collection->column_ids()) {
        if (const auto value = find_column(id)) {
            result.push_back(*value);
        }
    }
    return result;
}

std::vector<std::reference_wrapper<const entry::IndexEntry>> CatalogState::list_indexes(
    common::CollectionId collection_id
) const
{
    std::vector<std::reference_wrapper<const entry::IndexEntry>> result;
    const auto collection = find_collection(collection_id);
    if (!collection) {
        return result;
    }
    result.reserve(collection->index_ids().size());
    for (const auto id : collection->index_ids()) {
        if (const auto value = find_index(id)) {
            result.push_back(*value);
        }
    }
    return result;
}

std::vector<std::reference_wrapper<const entry::VectorIndexEntry>>
CatalogState::list_vector_indexes(common::CollectionId collection_id) const
{
    std::vector<std::reference_wrapper<const entry::VectorIndexEntry>> result;
    const auto collection = find_collection(collection_id);
    if (!collection) {
        return result;
    }
    result.reserve(collection->vector_index_ids().size());
    for (const auto id : collection->vector_index_ids()) {
        if (const auto value = find_vector_index(id)) {
            result.push_back(*value);
        }
    }
    return result;
}

std::expected<common::DatabaseId, CatalogError> CatalogState::create_database(
    const CreateDatabaseRequest & request
)
{
    if (empty(request.database_name)) {
        return std::unexpected(
            make_error(CatalogErrorCode::InvalidArgument, "Database name cannot be empty")
        );
    }
    const auto key = common::normalize_identifier(request.database_name);
    if (database_keys_.contains(key)) {
        return std::unexpected(make_error(
            CatalogErrorCode::DuplicateDatabase,
            "Database already exists: " + request.database_name
        ));
    }
    if (!can_allocate(next_database_id_)) {
        return std::unexpected(
            make_error(CatalogErrorCode::InvalidState, "Database ID space is exhausted")
        );
    }
    const auto id = next_database_id_++;
    auto database = std::make_unique<entry::DatabaseEntry>(id, request.database_name);
    database_keys_.emplace(database->key(), id);
    databases_.emplace(id, std::move(database));
    database_ids_.push_back(id);
    return id;
}

std::expected<void, CatalogError> CatalogState::drop_database(const DropDatabaseRequest & request)
{
    const auto database = find_database(request.database_id);
    if (!database) {
        return std::unexpected(
            make_error(CatalogErrorCode::DatabaseNotFound, "Database not found")
        );
    }
    const std::string key = database->key();
    const std::vector<common::CollectionId> collection_ids {
        database->collection_ids().begin(),
        database->collection_ids().end(),
    };

    const auto database_key = database_keys_.find(key);
    if (database_key == database_keys_.end() || database_key->second != request.database_id) {
        return std::unexpected(
            make_error(CatalogErrorCode::InvalidState, "Database name index is inconsistent")
        );
    }
    if (std::ranges::count(database_ids_, request.database_id) != 1) {
        return std::unexpected(
            make_error(CatalogErrorCode::InvalidState, "Database ID index is inconsistent")
        );
    }
    for (const auto collection_id : collection_ids) {
        const auto collection = find_collection(collection_id);
        if (!collection || collection->database_id() != request.database_id) {
            return std::unexpected(make_error(
                CatalogErrorCode::InvalidState,
                "Database collection index is inconsistent"
            ));
        }
        if (auto validated = validate_collection_for_erase(collection_id); !validated) {
            return validated;
        }
    }
    for (const auto collection_id : collection_ids) {
        erase_collection(collection_id);
    }
    databases_.erase(request.database_id);
    database_keys_.erase(key);
    std::erase(database_ids_, request.database_id);
    return {};
}

std::expected<common::CollectionId, CatalogError> CatalogState::create_collection(
    const CreateCollectionRequest & request
)
{
    const auto database = find_database_mutable(request.database_id);
    if (!database) {
        return std::unexpected(
            make_error(CatalogErrorCode::DatabaseNotFound, "Database not found")
        );
    }
    if (empty(request.collection_name)) {
        return std::unexpected(
            make_error(CatalogErrorCode::InvalidArgument, "Collection name cannot be empty")
        );
    }
    const auto key = common::normalize_identifier(request.collection_name);
    if (const auto existing = database->find_collection_id(key)) {
        return std::unexpected(make_error(
            CatalogErrorCode::DuplicateCollection,
            "Collection already exists: " + request.collection_name
        ));
    }
    if (request.columns.empty()) {
        return std::unexpected(make_error(
            CatalogErrorCode::InvalidArgument,
            "Collection must have at least one column"
        ));
    }
    std::unordered_set<std::string> column_keys;
    std::size_t unique_column_count = 0;
    for (const auto & column : request.columns) {
        if (empty(column.name)) {
            return std::unexpected(
                make_error(CatalogErrorCode::InvalidArgument, "Column name cannot be empty")
            );
        }
        if (!column_keys.insert(common::normalize_identifier(column.name)).second) {
            return std::unexpected(
                make_error(CatalogErrorCode::DuplicateColumn, "Duplicate column: " + column.name)
            );
        }
        if (!valid_logical_type(column.type) ||
            (column.default_expression && !valid_default_expression(*column.default_expression))) {
            return std::unexpected(make_error(
                CatalogErrorCode::InvalidArgument,
                "Invalid column type or default expression: " + column.name
            ));
        }
        if (column.unique && column.type.id == common::LogicalTypeId::Vector) {
            return std::unexpected(make_error(
                CatalogErrorCode::InvalidArgument,
                "VECTOR columns cannot have a UNIQUE constraint: " + column.name
            ));
        }
        unique_column_count += column.unique ? 1U : 0U;
    }
    if (!can_allocate(next_collection_id_) ||
        !can_allocate(next_column_id_, request.columns.size()) ||
        !can_allocate(next_index_id_, unique_column_count)) {
        return std::unexpected(make_error(
            CatalogErrorCode::InvalidState,
            "Collection, column, or implicit unique index ID space is exhausted"
        ));
    }
    const auto id = next_collection_id_++;
    auto collection = std::make_unique<entry::CollectionEntry>(
        id,
        request.database_id,
        request.collection_name,
        request.comment
    );
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
        if (definition.unique) {
            const auto index_id = next_index_id_++;
            auto index = std::make_unique<entry::IndexEntry>(
                index_id,
                id,
                column_id,
                implicit_unique_index_name(column_id),
                entry::IndexKind::BTree,
                true
            );
            collection->add_index(index->key(), index_id);
            indexes_.emplace(index_id, std::move(index));
        }
    }
    database->add_collection(collection->key(), id);
    collections_.emplace(id, std::move(collection));
    return id;
}

void CatalogState::erase_collection(common::CollectionId id)
{
    const auto collection = collections_.find(id);
    assert(collection != collections_.end());
    if (collection == collections_.end()) {
        return;
    }

    for (const auto column_id : collection->second->column_ids()) {
        columns_.erase(column_id);
    }
    for (const auto index_id : collection->second->index_ids()) {
        indexes_.erase(index_id);
    }
    for (const auto vector_index_id : collection->second->vector_index_ids()) {
        vector_indexes_.erase(vector_index_id);
    }
    collections_.erase(collection);
}

std::expected<void, CatalogError> CatalogState::validate_collection_for_erase(
    common::CollectionId id
) const
{
    const auto collection = find_collection(id);
    if (!collection) {
        return std::unexpected(
            make_error(CatalogErrorCode::InvalidState, "Collection entry is missing")
        );
    }
    for (const auto column_id : collection->column_ids()) {
        const auto column = find_column(column_id);
        if (!column || column->collection_id() != id) {
            return std::unexpected(make_error(
                CatalogErrorCode::InvalidState,
                "Collection column index is inconsistent"
            ));
        }
        const auto indexed_column_id = collection->find_column_id(column->key());
        if (!indexed_column_id || *indexed_column_id != column_id) {
            return std::unexpected(make_error(
                CatalogErrorCode::InvalidState,
                "Collection column name index is inconsistent"
            ));
        }
    }
    for (const auto index_id : collection->index_ids()) {
        const auto index = find_index(index_id);
        if (!index || index->collection_id() != id) {
            return std::unexpected(make_error(
                CatalogErrorCode::InvalidState,
                "Collection index reference is inconsistent"
            ));
        }
        const auto indexed_index_id = collection->find_index_id(index->key());
        if (!indexed_index_id || *indexed_index_id != index_id) {
            return std::unexpected(make_error(
                CatalogErrorCode::InvalidState,
                "Collection index name map is inconsistent"
            ));
        }
        const auto column = find_column(index->column_id());
        if (!column || column->collection_id() != id) {
            return std::unexpected(
                make_error(CatalogErrorCode::InvalidState, "Index column reference is inconsistent")
            );
        }
    }
    for (const auto vector_index_id : collection->vector_index_ids()) {
        const auto index = find_vector_index(vector_index_id);
        if (!index || index->collection_id() != id) {
            return std::unexpected(make_error(
                CatalogErrorCode::InvalidState,
                "Collection vector index reference is inconsistent"
            ));
        }
        const auto indexed_index_id = collection->find_vector_index_id(index->key());
        if (!indexed_index_id || *indexed_index_id != vector_index_id) {
            return std::unexpected(make_error(
                CatalogErrorCode::InvalidState,
                "Collection vector index name map is inconsistent"
            ));
        }
        const auto column = find_column(index->column_id());
        if (!column || column->collection_id() != id) {
            return std::unexpected(make_error(
                CatalogErrorCode::InvalidState,
                "Vector index column reference is inconsistent"
            ));
        }
    }
    return {};
}

std::expected<void, CatalogError> CatalogState::drop_collection(
    const DropCollectionRequest & request
)
{
    const auto collection = find_collection(request.collection_id);
    if (!collection) {
        return std::unexpected(
            make_error(CatalogErrorCode::CollectionNotFound, "Collection not found")
        );
    }

    const auto database_id = collection->database_id();
    const std::string collection_key = collection->key();

    const auto database = find_database_mutable(database_id);
    if (!database) {
        return std::unexpected(
            make_error(CatalogErrorCode::InvalidState, "Collection parent database not found")
        );
    }
    const auto indexed_collection_id = database->find_collection_id(collection_key);
    if (!indexed_collection_id || *indexed_collection_id != request.collection_id) {
        return std::unexpected(
            make_error(CatalogErrorCode::InvalidState, "Database collection index is inconsistent")
        );
    }
    if (auto validated = validate_collection_for_erase(request.collection_id); !validated) {
        return validated;
    }

    database->remove_collection(collection_key);
    erase_collection(request.collection_id);
    return {};
}

std::expected<common::IndexId, CatalogError> CatalogState::create_index(
    const CreateIndexRequest & request
)
{
    const auto collection = find_collection_mutable(request.collection_id);
    if (!collection) {
        return std::unexpected(
            make_error(CatalogErrorCode::CollectionNotFound, "Collection not found")
        );
    }
    if (empty(request.index_name)) {
        return std::unexpected(
            make_error(CatalogErrorCode::InvalidArgument, "Index name cannot be empty")
        );
    }
    const auto key = common::normalize_identifier(request.index_name);
    if (collection->contains_index(key)) {
        return std::unexpected(make_error(
            CatalogErrorCode::DuplicateIndex,
            "Index already exists: " + request.index_name
        ));
    }
    if (collection->contains_vector_index(key)) {
        return std::unexpected(make_error(
            CatalogErrorCode::DuplicateIndex,
            "Index name already exists: " + request.index_name
        ));
    }
    if (request.kind != entry::IndexKind::BTree) {
        return std::unexpected(
            make_error(CatalogErrorCode::InvalidArgument, "Scalar index kind must be BTREE")
        );
    }
    const auto column = find_column(request.column_id);
    if (!column || column->collection_id() != request.collection_id) {
        return std::unexpected(
            make_error(CatalogErrorCode::ColumnNotFound, "Index column not found")
        );
    }
    if (column->type().id == common::LogicalTypeId::Vector) {
        return std::unexpected(make_error(
            CatalogErrorCode::InvalidArgument,
            "Scalar index cannot contain a VECTOR column"
        ));
    }
    if (!can_allocate(next_index_id_)) {
        return std::unexpected(
            make_error(CatalogErrorCode::InvalidState, "Index ID space is exhausted")
        );
    }
    const auto id = next_index_id_++;
    auto index = std::make_unique<entry::IndexEntry>(
        id,
        request.collection_id,
        request.column_id,
        request.index_name,
        request.kind,
        request.unique
    );
    collection->add_index(index->key(), id);
    indexes_.emplace(id, std::move(index));
    return id;
}

std::expected<void, CatalogError> CatalogState::drop_index(const DropIndexRequest & request)
{
    const auto index = find_index(request.index_id);
    if (!index) {
        return std::unexpected(make_error(CatalogErrorCode::IndexNotFound, "Index not found"));
    }
    const auto collection = find_collection_mutable(index->collection_id());
    if (!collection) {
        return std::unexpected(
            make_error(CatalogErrorCode::InvalidState, "Index parent collection not found")
        );
    }
    const std::string key = index->key();
    const auto indexed_index_id = collection->find_index_id(key);
    if (!indexed_index_id || *indexed_index_id != request.index_id) {
        return std::unexpected(
            make_error(CatalogErrorCode::InvalidState, "Collection index map is inconsistent")
        );
    }
    const auto column_id = index->column_id();
    const auto column = find_column(column_id);
    if (!column || column->collection_id() != index->collection_id()) {
        return std::unexpected(
            make_error(CatalogErrorCode::InvalidState, "Index column reference is inconsistent")
        );
    }
    if (column->unique() && index->unique() &&
        index->name() == implicit_unique_index_name(column_id)) {
        return std::unexpected(make_error(
            CatalogErrorCode::InvalidArgument,
            "Implicit UNIQUE indexes cannot be dropped"
        ));
    }
    collection->remove_index(key);
    indexes_.erase(request.index_id);
    return {};
}

std::expected<common::VIndexId, CatalogError> CatalogState::create_vector_index(
    const CreateVectorIndexRequest & request
)
{
    const auto collection = find_collection_mutable(request.collection_id);
    if (!collection) {
        return std::unexpected(
            make_error(CatalogErrorCode::CollectionNotFound, "Collection not found")
        );
    }
    if (empty(request.vector_index_name)) {
        return std::unexpected(
            make_error(CatalogErrorCode::InvalidArgument, "Vector index name cannot be empty")
        );
    }
    const auto key = common::normalize_identifier(request.vector_index_name);
    if (collection->contains_vector_index(key)) {
        return std::unexpected(make_error(
            CatalogErrorCode::DuplicateVectorIndex,
            "Vector index already exists: " + request.vector_index_name
        ));
    }
    if (collection->contains_index(key)) {
        return std::unexpected(make_error(
            CatalogErrorCode::DuplicateVectorIndex,
            "Index name already exists: " + request.vector_index_name
        ));
    }
    const auto column = find_column(request.column_id);
    if (!column || column->collection_id() != request.collection_id) {
        return std::unexpected(
            make_error(CatalogErrorCode::ColumnNotFound, "Vector index column not found")
        );
    }
    if (column->type().id != common::LogicalTypeId::Vector || !column->type().parameter ||
        *column->type().parameter == 0) {
        return std::unexpected(make_error(
            CatalogErrorCode::InvalidArgument,
            "Vector index requires a VECTOR(n) column"
        ));
    }
    if (request.kind != entry::VectorIndexKind::Hnsw ||
        static_cast<std::uint8_t>(request.metric) >
            static_cast<std::uint8_t>(entry::VectorDistanceMetric::Cosine)) {
        return std::unexpected(
            make_error(CatalogErrorCode::InvalidArgument, "Invalid vector index kind or metric")
        );
    }
    if (request.hnsw_options.max_neighbors == 0 ||
        request.hnsw_options.ef_construction < request.hnsw_options.max_neighbors ||
        request.hnsw_options.ef_search_default == 0) {
        return std::unexpected(
            make_error(CatalogErrorCode::InvalidArgument, "Invalid HNSW options")
        );
    }
    if (!can_allocate(next_vector_index_id_)) {
        return std::unexpected(
            make_error(CatalogErrorCode::InvalidState, "Vector index ID space is exhausted")
        );
    }
    const auto id = next_vector_index_id_++;
    auto index = std::make_unique<entry::VectorIndexEntry>(
        id,
        request.collection_id,
        request.column_id,
        request.vector_index_name,
        request.kind,
        request.metric,
        *column->type().parameter,
        request.hnsw_options
    );
    collection->add_vector_index(index->key(), id);
    vector_indexes_.emplace(id, std::move(index));
    return id;
}

std::expected<void, CatalogError> CatalogState::drop_vector_index(
    const DropVectorIndexRequest & request
)
{
    const auto index = find_vector_index(request.vector_index_id);
    if (!index) {
        return std::unexpected(
            make_error(CatalogErrorCode::VectorIndexNotFound, "Vector index not found")
        );
    }
    const auto collection = find_collection_mutable(index->collection_id());
    if (!collection) {
        return std::unexpected(
            make_error(CatalogErrorCode::InvalidState, "Vector index parent collection not found")
        );
    }
    const std::string key = index->key();
    const auto indexed_index_id = collection->find_vector_index_id(key);
    if (!indexed_index_id || *indexed_index_id != request.vector_index_id) {
        return std::unexpected(make_error(
            CatalogErrorCode::InvalidState,
            "Collection vector index map is inconsistent"
        ));
    }
    const auto column = find_column(index->column_id());
    if (!column || column->collection_id() != index->collection_id()) {
        return std::unexpected(make_error(
            CatalogErrorCode::InvalidState,
            "Vector index column reference is inconsistent"
        ));
    }
    collection->remove_vector_index(key);
    vector_indexes_.erase(request.vector_index_id);
    return {};
}

CatalogSnapshot CatalogState::snapshot() const
{
    CatalogSnapshot result;
    result.next_database_id = next_database_id_;
    result.next_collection_id = next_collection_id_;
    result.next_column_id = next_column_id_;
    result.next_index_id = next_index_id_;
    result.next_vector_index_id = next_vector_index_id_;
    for (const auto & database_reference : list_databases()) {
        const auto & database = database_reference.get();
        CatalogDatabaseSnapshot database_snapshot {database.id(), database.name(), {}};
        for (const auto & collection_reference : list_collections(database.id())) {
            const auto & collection = collection_reference.get();
            CatalogCollectionSnapshot collection_snapshot;
            collection_snapshot.id = collection.id();
            collection_snapshot.database_id = database.id();
            collection_snapshot.name = collection.name();
            if (const auto comment = collection.comment()) {
                collection_snapshot.comment = *comment;
            }
            for (const auto & column_reference : list_columns(collection.id())) {
                const auto & column = column_reference.get();
                CatalogColumnSnapshot column_snapshot;
                column_snapshot.id = column.id();
                column_snapshot.name = column.name();
                column_snapshot.type = column.type();
                column_snapshot.unique = column.unique();
                column_snapshot.nullable = column.nullable();
                if (const auto default_expression = column.default_expression()) {
                    column_snapshot.default_expression = *default_expression;
                }
                if (const auto comment = column.comment()) {
                    column_snapshot.comment = *comment;
                }
                collection_snapshot.columns.push_back(std::move(column_snapshot));
            }
            for (const auto & index_reference : list_indexes(collection.id())) {
                const auto & index = index_reference.get();
                collection_snapshot.indexes.push_back({
                    index.id(),
                    index.column_id(),
                    index.name(),
                    index.kind(),
                    index.unique(),
                });
            }
            for (const auto & index_reference : list_vector_indexes(collection.id())) {
                const auto & index = index_reference.get();
                collection_snapshot.vector_indexes.push_back({
                    index.id(),
                    index.column_id(),
                    index.name(),
                    index.index_kind(),
                    index.metric(),
                    index.dimension(),
                    index.max_neighbors(),
                    index.ef_construction(),
                    index.ef_search_default(),
                    index.random_seed(),
                });
            }
            database_snapshot.collections.push_back(std::move(collection_snapshot));
        }
        result.databases.push_back(std::move(database_snapshot));
    }
    return result;
}

std::expected<void, CatalogError> CatalogState::restore(const CatalogSnapshot & source)
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
        if (database_snapshot.id == 0 || empty(database_snapshot.name)) {
            return std::unexpected(make_error(
                CatalogErrorCode::InvalidSnapshot,
                "Invalid database in catalog snapshot"
            ));
        }
        auto database =
            std::make_unique<entry::DatabaseEntry>(database_snapshot.id, database_snapshot.name);
        if (rebuilt.databases_.contains(database_snapshot.id) ||
            rebuilt.database_keys_.contains(database->key())) {
            return std::unexpected(make_error(
                CatalogErrorCode::InvalidSnapshot,
                "Duplicate database in catalog snapshot"
            ));
        }
        auto * database_ptr = database.get();
        rebuilt.database_ids_.push_back(database_snapshot.id);
        rebuilt.database_keys_.emplace(database->key(), database_snapshot.id);
        rebuilt.databases_.emplace(database_snapshot.id, std::move(database));
        max_database = std::max(max_database, database_snapshot.id);
        for (const auto & collection_snapshot : database_snapshot.collections) {
            if (collection_snapshot.id == 0 ||
                collection_snapshot.database_id != database_snapshot.id ||
                empty(collection_snapshot.name)) {
                return std::unexpected(make_error(
                    CatalogErrorCode::InvalidSnapshot,
                    "Invalid collection in catalog snapshot"
                ));
            }
            if (collection_snapshot.columns.empty()) {
                return std::unexpected(make_error(
                    CatalogErrorCode::InvalidSnapshot,
                    "Collection snapshot must contain columns"
                ));
            }
            auto collection = std::make_unique<entry::CollectionEntry>(
                collection_snapshot.id,
                database_snapshot.id,
                collection_snapshot.name,
                collection_snapshot.comment
            );
            if (rebuilt.collections_.contains(collection_snapshot.id) ||
                database_ptr->contains_collection(collection->key())) {
                return std::unexpected(make_error(
                    CatalogErrorCode::InvalidSnapshot,
                    "Duplicate collection in catalog snapshot"
                ));
            }
            auto * collection_ptr = collection.get();
            std::unordered_set<common::ColumnId> collection_columns;
            for (std::size_t ordinal = 0; ordinal < collection_snapshot.columns.size(); ++ordinal) {
                const auto & value = collection_snapshot.columns[ordinal];
                if (value.id == 0 || empty(value.name) || rebuilt.columns_.contains(value.id) ||
                    collection_ptr->contains_column(common::normalize_identifier(value.name))) {
                    return std::unexpected(make_error(
                        CatalogErrorCode::InvalidSnapshot,
                        "Invalid or duplicate column in catalog snapshot"
                    ));
                }
                if (!valid_logical_type(value.type) ||
                    (value.default_expression &&
                     !valid_default_expression(*value.default_expression)) ||
                    (value.unique && value.type.id == common::LogicalTypeId::Vector)) {
                    return std::unexpected(make_error(
                        CatalogErrorCode::InvalidSnapshot,
                        "Invalid column type or default expression in catalog snapshot"
                    ));
                }
                auto column = std::make_unique<entry::ColumnEntry>(
                    value.id,
                    collection_snapshot.id,
                    ordinal,
                    value.name,
                    value.type,
                    value.unique,
                    value.nullable,
                    value.default_expression,
                    value.comment
                );
                collection_ptr->add_column(column->key(), value.id);
                rebuilt.columns_.emplace(value.id, std::move(column));
                collection_columns.insert(value.id);
                max_column = std::max(max_column, value.id);
            }
            for (const auto & value : collection_snapshot.indexes) {
                if (value.id == 0 || value.column_id == 0 || empty(value.name) ||
                    rebuilt.indexes_.contains(value.id) ||
                    collection_ptr->contains_index(common::normalize_identifier(value.name)) ||
                    value.index_kind != entry::IndexKind::BTree) {
                    return std::unexpected(make_error(
                        CatalogErrorCode::InvalidSnapshot,
                        "Invalid or duplicate index in catalog snapshot"
                    ));
                }
                const auto column = rebuilt.find_column(value.column_id);
                if (!collection_columns.contains(value.column_id) || !column) {
                    return std::unexpected(make_error(
                        CatalogErrorCode::InvalidSnapshot,
                        "Index column not found in catalog snapshot"
                    ));
                }
                if (column->type().id == common::LogicalTypeId::Vector) {
                    return std::unexpected(make_error(
                        CatalogErrorCode::InvalidSnapshot,
                        "Invalid scalar index column in catalog snapshot"
                    ));
                }
                auto index = std::make_unique<entry::IndexEntry>(
                    value.id,
                    collection_snapshot.id,
                    value.column_id,
                    value.name,
                    value.index_kind,
                    value.unique
                );
                collection_ptr->add_index(index->key(), value.id);
                rebuilt.indexes_.emplace(value.id, std::move(index));
                max_index = std::max(max_index, value.id);
            }
            for (const auto & value : collection_snapshot.vector_indexes) {
                if (value.id == 0 || value.column_id == 0 || empty(value.name) ||
                    rebuilt.vector_indexes_.contains(value.id) ||
                    collection_ptr->contains_index(common::normalize_identifier(value.name)) ||
                    collection_ptr->contains_vector_index(
                        common::normalize_identifier(value.name)
                    ) ||
                    !collection_columns.contains(value.column_id) ||
                    value.index_kind != entry::VectorIndexKind::Hnsw ||
                    static_cast<std::uint8_t>(value.metric) >
                        static_cast<std::uint8_t>(entry::VectorDistanceMetric::Cosine)) {
                    return std::unexpected(make_error(
                        CatalogErrorCode::InvalidSnapshot,
                        "Invalid or duplicate vector index in catalog snapshot"
                    ));
                }
                const auto column = rebuilt.find_column(value.column_id);
                if (!column || column->type().id != common::LogicalTypeId::Vector ||
                    !column->type().parameter || *column->type().parameter != value.dimension ||
                    value.max_neighbors == 0 || value.ef_construction < value.max_neighbors ||
                    value.ef_search_default == 0) {
                    return std::unexpected(make_error(
                        CatalogErrorCode::InvalidSnapshot,
                        "Invalid vector index options in catalog snapshot"
                    ));
                }
                entry::HnswOptions options {
                    value.max_neighbors,
                    value.ef_construction,
                    value.ef_search_default,
                    value.random_seed,
                };
                auto index = std::make_unique<entry::VectorIndexEntry>(
                    value.id,
                    collection_snapshot.id,
                    value.column_id,
                    value.name,
                    value.index_kind,
                    value.metric,
                    value.dimension,
                    options
                );
                collection_ptr->add_vector_index(index->key(), value.id);
                rebuilt.vector_indexes_.emplace(value.id, std::move(index));
                max_vector_index = std::max(max_vector_index, value.id);
            }
            database_ptr->add_collection(collection->key(), collection_snapshot.id);
            rebuilt.collections_.emplace(collection_snapshot.id, std::move(collection));
            max_collection = std::max(max_collection, collection_snapshot.id);
        }
    }
    if (source.next_database_id <= max_database || source.next_collection_id <= max_collection ||
        source.next_column_id <= max_column || source.next_index_id <= max_index ||
        source.next_vector_index_id <= max_vector_index) {
        return std::unexpected(make_error(
            CatalogErrorCode::InvalidSnapshot,
            "Catalog snapshot next id is behind existing ids"
        ));
    }

    *this = std::move(rebuilt);
    return {};
}

std::expected<CatalogState, CatalogError> build_catalog_state(const CatalogSnapshot & snapshot)
{
    CatalogState state;
    if (auto restored = state.restore(snapshot); !restored) {
        return std::unexpected(std::move(restored.error()));
    }
    return state;
}

} // namespace litedb::core::catalog
