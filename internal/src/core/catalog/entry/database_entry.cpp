#include "core/catalog/entry/database_entry.hpp"

#include <utility>

namespace litedb::core::catalog::entry
{

DatabaseEntry::DatabaseEntry(common::DatabaseId id, std::string name)
    : CatalogEntry(CatalogEntryKind::Database, id, std::move(name))
{
}

common::DatabaseId DatabaseEntry::id() const noexcept
{
    return raw_id();
}

std::span<const common::CollectionId> DatabaseEntry::collection_ids() const noexcept
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

bool DatabaseEntry::contains_collection(std::string_view collection_key) const
{
    return collections_by_key_.contains(std::string(collection_key));
}

void DatabaseEntry::add_collection(std::string_view collection_key, common::CollectionId collection_id)
{
    const auto [_, inserted] = collections_by_key_.emplace(std::string(collection_key), collection_id);
    if (inserted) {
        collection_ids_.push_back(collection_id);
    }
}

void DatabaseEntry::remove_collection(std::string_view collection_key)
{
    const auto it = collections_by_key_.find(std::string(collection_key));
    if (it == collections_by_key_.end()) {
        return;
    }
    const auto collection_id = it->second;
    collections_by_key_.erase(it);
    std::erase(collection_ids_, collection_id);
}

} // namespace litedb::core::catalog::entry
