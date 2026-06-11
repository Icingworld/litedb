#include "core/storage/storage_manager.hpp"

#include <utility>

#include "core/storage/in_memory_collection_storage.hpp"

namespace litedb::core::storage
{

namespace
{

StorageError make_error(StorageErrorCode code, std::string message)
{
    return StorageError {code, std::move(message)};
}

} // namespace

StorageManager::StorageManager()
    : StorageManager(
          [](schema::CollectionSchema collection_schema) {
              return std::make_unique<InMemoryCollectionStorage>(std::move(collection_schema));
          }
      )
{
}

StorageManager::StorageManager(CollectionFactory collection_factory)
    : collection_factory_(std::move(collection_factory))
{
}

std::expected<void, StorageError> StorageManager::create_collection(schema::CollectionSchema collection_schema)
{
    const auto collection_id = collection_schema.collection_id();
    if (collections_.contains(collection_id)) {
        return std::unexpected(make_error(StorageErrorCode::CollectionAlreadyExists, "Collection storage already exists"));
    }

    collections_.emplace(collection_id, collection_factory_(std::move(collection_schema)));
    return {};
}

std::expected<void, StorageError> StorageManager::register_collection(
    common::CollectionId collection_id,
    std::unique_ptr<CollectionStorage> collection_storage
)
{
    if (collection_storage == nullptr) {
        return std::unexpected(make_error(StorageErrorCode::InvalidStorageState, "Collection storage cannot be null"));
    }
    if (collections_.contains(collection_id)) {
        return std::unexpected(make_error(StorageErrorCode::CollectionAlreadyExists, "Collection storage already exists"));
    }

    collections_.emplace(collection_id, std::move(collection_storage));
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

void StorageManager::clear() noexcept
{
    collections_.clear();
}

} // namespace litedb::core::storage
