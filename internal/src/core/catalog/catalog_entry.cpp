#include "core/catalog/catalog_entry.hpp"

#include <cctype>
#include <utility>

namespace litedb::core::catalog
{

std::string normalize_identifier(std::string_view name)
{
    std::string key;
    key.reserve(name.size());
    for (const unsigned char ch : name) {
        key.push_back(static_cast<char>(std::tolower(ch)));
    }
    return key;
}

CatalogEntry::CatalogEntry(CatalogEntryKind kind, std::uint64_t id, std::string name)
    : kind_(kind),
      id_(id),
      name_(std::move(name)),
      key_(normalize_identifier(name_))
{
}

CatalogEntryKind CatalogEntry::kind() const noexcept
{
    return kind_;
}

std::uint64_t CatalogEntry::raw_id() const noexcept
{
    return id_;
}

const std::string & CatalogEntry::name() const noexcept
{
    return name_;
}

const std::string & CatalogEntry::key() const noexcept
{
    return key_;
}

DatabaseEntry::DatabaseEntry(common::DatabaseId id, std::string name)
    : CatalogEntry(CatalogEntryKind::Database, id, std::move(name))
{
}

common::DatabaseId DatabaseEntry::id() const noexcept
{
    return raw_id();
}

const std::vector<common::CollectionId> & DatabaseEntry::collection_ids() const noexcept
{
    return collection_ids_;
}

std::optional<common::CollectionId> DatabaseEntry::find_collection_id(std::string_view collection_key) const
{
    const auto it = collections_by_key_.find(std::string(collection_key));
    if (it == collections_by_key_.end()) {
        return std::nullopt;
    }
    return it->second;
}

void DatabaseEntry::add_collection(std::string_view collection_key, common::CollectionId collection_id)
{
    collections_by_key_.emplace(std::string(collection_key), collection_id);
    collection_ids_.push_back(collection_id);
}

void DatabaseEntry::remove_collection(std::string_view collection_key, common::CollectionId collection_id)
{
    collections_by_key_.erase(std::string(collection_key));
    std::erase(collection_ids_, collection_id);
}

CollectionEntry::CollectionEntry(common::CollectionId id, common::DatabaseId database_id, std::string name, std::optional<std::string> comment)
    : CatalogEntry(CatalogEntryKind::Collection, id, std::move(name)),
      database_id_(database_id),
      comment_(std::move(comment))
{
}

common::CollectionId CollectionEntry::id() const noexcept
{
    return raw_id();
}

common::DatabaseId CollectionEntry::database_id() const noexcept
{
    return database_id_;
}

const std::vector<common::ColumnId> & CollectionEntry::column_ids() const noexcept
{
    return column_ids_;
}

const std::vector<common::IndexId> & CollectionEntry::index_ids() const noexcept
{
    return index_ids_;
}

const std::vector<common::VIndexId> & CollectionEntry::vector_index_ids() const noexcept
{
    return vector_index_ids_;
}

std::optional<common::ColumnId> CollectionEntry::primary_key_column_id() const noexcept
{
    return primary_key_column_id_;
}

const std::optional<std::string> & CollectionEntry::comment() const noexcept
{
    return comment_;
}

std::optional<common::ColumnId> CollectionEntry::find_column_id(std::string_view column_key) const
{
    const auto it = columns_by_key_.find(std::string(column_key));
    if (it == columns_by_key_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<common::IndexId> CollectionEntry::find_index_id(std::string_view index_key) const
{
    const auto it = indexes_by_key_.find(std::string(index_key));
    if (it == indexes_by_key_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<common::VIndexId> CollectionEntry::find_vector_index_id(std::string_view index_key) const
{
    const auto it = vector_indexes_by_key_.find(std::string(index_key));
    if (it == vector_indexes_by_key_.end()) {
        return std::nullopt;
    }
    return it->second;
}

void CollectionEntry::add_column(std::string_view column_key, common::ColumnId column_id, bool primary_key)
{
    columns_by_key_.emplace(std::string(column_key), column_id);
    column_ids_.push_back(column_id);
    if (primary_key) {
        primary_key_column_id_ = column_id;
    }
}

void CollectionEntry::add_index(std::string_view index_key, common::IndexId index_id)
{
    indexes_by_key_.emplace(std::string(index_key), index_id);
    index_ids_.push_back(index_id);
}

void CollectionEntry::add_vector_index(std::string_view index_key, common::VIndexId index_id)
{
    vector_indexes_by_key_.emplace(std::string(index_key), index_id);
    vector_index_ids_.push_back(index_id);
}

void CollectionEntry::remove_index(std::string_view index_key, common::IndexId index_id)
{
    indexes_by_key_.erase(std::string(index_key));
    std::erase(index_ids_, index_id);
}

void CollectionEntry::remove_vector_index(std::string_view index_key, common::VIndexId index_id)
{
    vector_indexes_by_key_.erase(std::string(index_key));
    std::erase(vector_index_ids_, index_id);
}

ColumnEntry::ColumnEntry(
    common::ColumnId id,
    common::CollectionId collection_id,
    std::string name,
    common::LogicalType type,
    bool primary_key,
    bool unique,
    bool nullable,
    std::optional<CatalogDefaultExpression> default_expression,
    std::optional<std::string> comment
)
    : CatalogEntry(CatalogEntryKind::Column, id, std::move(name)),
      collection_id_(collection_id),
      type_(std::move(type)),
      primary_key_(primary_key),
      unique_(unique),
      nullable_(primary_key ? false : nullable),
      default_expression_(std::move(default_expression)),
      comment_(std::move(comment))
{
}

common::ColumnId ColumnEntry::id() const noexcept
{
    return raw_id();
}

common::CollectionId ColumnEntry::collection_id() const noexcept
{
    return collection_id_;
}

const common::LogicalType & ColumnEntry::type() const noexcept
{
    return type_;
}

bool ColumnEntry::primary_key() const noexcept
{
    return primary_key_;
}

bool ColumnEntry::unique() const noexcept
{
    return unique_;
}

bool ColumnEntry::nullable() const noexcept
{
    return nullable_;
}

const std::optional<CatalogDefaultExpression> & ColumnEntry::default_expression() const noexcept
{
    return default_expression_;
}

const std::optional<std::string> & ColumnEntry::comment() const noexcept
{
    return comment_;
}

IndexEntry::IndexEntry(
    common::IndexId id,
    common::CollectionId collection_id,
    common::ColumnId column_id,
    std::string name,
    CatalogIndexKind index_kind,
    bool unique
)
    : CatalogEntry(CatalogEntryKind::Index, id, std::move(name)),
      collection_id_(collection_id),
      column_id_(column_id),
      index_kind_(index_kind),
      unique_(unique)
{
}

common::IndexId IndexEntry::id() const noexcept
{
    return raw_id();
}

common::CollectionId IndexEntry::collection_id() const noexcept
{
    return collection_id_;
}

common::ColumnId IndexEntry::column_id() const noexcept
{
    return column_id_;
}

CatalogIndexKind IndexEntry::index_kind() const noexcept
{
    return index_kind_;
}

bool IndexEntry::unique() const noexcept
{
    return unique_;
}

VectorIndexEntry::VectorIndexEntry(
    common::VIndexId id,
    common::CollectionId collection_id,
    common::ColumnId column_id,
    std::string name,
    CatalogVectorIndexKind index_kind,
    CatalogVectorDistanceMetric metric,
    std::size_t dimension,
    std::size_t max_neighbors,
    std::size_t ef_construction,
    std::size_t ef_search_default,
    std::size_t random_seed
)
    : CatalogEntry(CatalogEntryKind::VectorIndex, id, std::move(name)),
      collection_id_(collection_id),
      column_id_(column_id),
      index_kind_(index_kind),
      metric_(metric),
      dimension_(dimension),
      max_neighbors_(max_neighbors),
      ef_construction_(ef_construction),
      ef_search_default_(ef_search_default),
      random_seed_(random_seed)
{
}

common::VIndexId VectorIndexEntry::id() const noexcept
{
    return raw_id();
}

common::CollectionId VectorIndexEntry::collection_id() const noexcept
{
    return collection_id_;
}

common::ColumnId VectorIndexEntry::column_id() const noexcept
{
    return column_id_;
}

CatalogVectorIndexKind VectorIndexEntry::index_kind() const noexcept
{
    return index_kind_;
}

CatalogVectorDistanceMetric VectorIndexEntry::metric() const noexcept
{
    return metric_;
}

std::size_t VectorIndexEntry::dimension() const noexcept
{
    return dimension_;
}

std::size_t VectorIndexEntry::max_neighbors() const noexcept
{
    return max_neighbors_;
}

std::size_t VectorIndexEntry::ef_construction() const noexcept
{
    return ef_construction_;
}

std::size_t VectorIndexEntry::ef_search_default() const noexcept
{
    return ef_search_default_;
}

std::size_t VectorIndexEntry::random_seed() const noexcept
{
    return random_seed_;
}

} // namespace litedb::core::catalog
