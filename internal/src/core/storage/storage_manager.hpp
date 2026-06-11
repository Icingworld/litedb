#pragma once

#include <expected>
#include <functional>
#include <memory>
#include <unordered_map>

#include "core/common/ids.hpp"
#include "core/schema/collection.hpp"
#include "core/storage/collection_storage.hpp"
#include "core/storage/storage_error.hpp"

namespace litedb::core::storage
{

class StorageManager
{
public:
    using CollectionFactory = std::function<std::unique_ptr<CollectionStorage>(schema::CollectionSchema)>;

public:
    StorageManager();

    explicit StorageManager(CollectionFactory collection_factory);

public:
    std::expected<void, StorageError> create_collection(schema::CollectionSchema collection_schema);

    std::expected<void, StorageError> register_collection(
        common::CollectionId collection_id,
        std::unique_ptr<CollectionStorage> collection_storage
    );

    std::expected<void, StorageError> drop_collection(common::CollectionId collection_id);

    [[nodiscard]]
    CollectionStorage * find_collection(common::CollectionId collection_id) noexcept;

    [[nodiscard]]
    const CollectionStorage * find_collection(common::CollectionId collection_id) const noexcept;

    void clear() noexcept;

private:
    CollectionFactory collection_factory_;
    std::unordered_map<common::CollectionId, std::unique_ptr<CollectionStorage>> collections_;
};

} // namespace litedb::core::storage
