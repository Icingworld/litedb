#include "core/storage/storage_manager.hpp"

#include <utility>

namespace litedb::core::storage
{

namespace
{

StorageError make_error(StorageErrorCode code, std::string message)
{
    return StorageError {code, std::move(message)};
}

} // namespace

std::expected<void, StorageError> StorageManager::create_collection(schema::CollectionSchema collection_schema)
{
    const auto collection_id = collection_schema.collection_id();
    if (collections_.contains(collection_id)) {
        return std::unexpected(make_error(StorageErrorCode::CollectionAlreadyExists, "Collection storage already exists"));
    }

    collections_.emplace(
        collection_id,
        std::make_unique<InMemoryCollectionStorage>(std::move(collection_schema))
    );
    return {};
}

std::expected<void, StorageError> StorageManager::drop_collection(common::CollectionId collection_id)
{
    const auto erased = collections_.erase(collection_id);
    if (erased == 0) {
        return std::unexpected(make_error(StorageErrorCode::CollectionNotFound, "Collection storage not found"));
    }
    return {};
}

CollectionStorage * StorageManager::find_collection(common::CollectionId collection_id) noexcept
{
    const auto it = collections_.find(collection_id);
    if (it == collections_.end()) {
        return nullptr;
    }
    return it->second.get();
}

const CollectionStorage * StorageManager::find_collection(common::CollectionId collection_id) const noexcept
{
    const auto it = collections_.find(collection_id);
    if (it == collections_.end()) {
        return nullptr;
    }
    return it->second.get();
}

} // namespace litedb::core::storage
