#include "core/catalog/catalog_entry.hpp"

#include <algorithm>
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

CollectionEntry::CollectionEntry(common::CollectionId id, common::DatabaseId database_id, std::string name)
    : CatalogEntry(CatalogEntryKind::Collection, id, std::move(name)),
      database_id_(database_id)
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

std::optional<common::ColumnId> CollectionEntry::primary_key_column_id() const noexcept
{
    return primary_key_column_id_;
}

std::optional<common::ColumnId> CollectionEntry::find_column_id(std::string_view column_key) const
{
    const auto it = columns_by_key_.find(std::string(column_key));
    if (it == columns_by_key_.end()) {
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

} // namespace litedb::core::catalog
