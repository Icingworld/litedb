#include "core/catalog/entry/collection_entry.hpp"

#include <utility>

namespace litedb::core::catalog::entry
{

CollectionEntry::CollectionEntry(
    common::CollectionId id,
    common::DatabaseId database_id,
    std::string name,
    std::optional<std::string> comment
)
    : CatalogEntry(CatalogEntryKind::Collection, id, std::move(name))
    , database_id_(database_id)
    , comment_(std::move(comment))
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

std::span<const common::ColumnId> CollectionEntry::column_ids() const noexcept
{
    return column_ids_;
}

std::span<const common::IndexId> CollectionEntry::index_ids() const noexcept
{
    return index_ids_;
}

std::span<const common::VIndexId> CollectionEntry::vector_index_ids() const noexcept
{
    return vector_index_ids_;
}

std::optional<const std::string &> CollectionEntry::comment() const noexcept
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

bool CollectionEntry::contains_column(std::string_view column_key) const
{
    return columns_by_key_.contains(std::string(column_key));
}

bool CollectionEntry::contains_index(std::string_view index_key) const
{
    return indexes_by_key_.contains(std::string(index_key));
}

bool CollectionEntry::contains_vector_index(std::string_view index_key) const
{
    return vector_indexes_by_key_.contains(std::string(index_key));
}

void CollectionEntry::add_column(std::string_view column_key, common::ColumnId column_id)
{
    const auto [_, inserted] = columns_by_key_.emplace(std::string(column_key), column_id);
    if (!inserted) {
        return;
    }

    column_ids_.push_back(column_id);
}

void CollectionEntry::remove_column(std::string_view column_key)
{
    const auto it = columns_by_key_.find(std::string(column_key));
    if (it == columns_by_key_.end()) {
        return;
    }
    const auto column_id = it->second;
    columns_by_key_.erase(it);
    std::erase(column_ids_, column_id);
}

void CollectionEntry::add_index(std::string_view index_key, common::IndexId index_id)
{
    const auto [_, inserted] = indexes_by_key_.emplace(std::string(index_key), index_id);
    if (inserted) {
        index_ids_.push_back(index_id);
    }
}

void CollectionEntry::remove_index(std::string_view index_key)
{
    const auto it = indexes_by_key_.find(std::string(index_key));
    if (it == indexes_by_key_.end()) {
        return;
    }
    const auto index_id = it->second;
    indexes_by_key_.erase(it);
    std::erase(index_ids_, index_id);
}

void CollectionEntry::add_vector_index(std::string_view index_key, common::VIndexId index_id)
{
    const auto [_, inserted] = vector_indexes_by_key_.emplace(std::string(index_key), index_id);
    if (inserted) {
        vector_index_ids_.push_back(index_id);
    }
}

void CollectionEntry::remove_vector_index(std::string_view index_key)
{
    const auto it = vector_indexes_by_key_.find(std::string(index_key));
    if (it == vector_indexes_by_key_.end()) {
        return;
    }
    const auto index_id = it->second;
    vector_indexes_by_key_.erase(it);
    std::erase(vector_index_ids_, index_id);
}

} // namespace litedb::core::catalog::entry
