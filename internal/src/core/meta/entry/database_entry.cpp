#include "core/meta/entry/database_entry.hpp"

#include <utility>

namespace litedb::core::meta::entry
{

DatabaseEntry::DatabaseEntry(common::DatabaseId id, std::string name)
    : MetaEntry(MetaEntryKind::Database, id, std::move(name))
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

void DatabaseEntry::remove_collection(std::string_view collection_key, common::CollectionId collection_id)
{
    collections_by_key_.erase(std::string(collection_key));
    std::erase(collection_ids_, collection_id);
}

} // namespace litedb::core::meta::entry
