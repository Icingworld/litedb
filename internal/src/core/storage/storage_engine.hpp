#pragma once

#include <expected>
#include <filesystem>
#include <memory>
#include <unordered_map>

#include "core/filesystem/filesystem.hpp"
#include "core/schema/collection.hpp"
#include "core/storage/storage_cursor.hpp"
#include "core/storage/storage_error.hpp"
#include "core/storage/storage_store.hpp"

namespace litedb::core::engine
{
class DatabaseInstance;
}

namespace litedb::core::storage
{

class StorageEngine
{
public:
    StorageEngine(std::filesystem::path data_directory, filesystem::FileSystem & filesystem) noexcept;
    ~StorageEngine();

    StorageEngine(const StorageEngine &) = delete;
    StorageEngine & operator=(const StorageEngine &) = delete;
    StorageEngine(StorageEngine &&) noexcept;
    StorageEngine & operator=(StorageEngine &&) noexcept;

    std::expected<void, StorageError> create_collection(schema::CollectionSchema schema);
    std::expected<void, StorageError> open_collection(schema::CollectionSchema schema);
    std::expected<void, StorageError> drop_collection(common::CollectionId collection_id);
    [[nodiscard]] bool contains_collection(common::CollectionId collection_id) const noexcept;
    [[nodiscard]] std::expected<schema::Record, StorageError> get(
        common::CollectionId collection_id, common::RecordId record_id) const;
    std::expected<common::RecordId, StorageError> insert(
        common::CollectionId collection_id, schema::RecordData data);
    std::expected<void, StorageError> update(
        common::CollectionId collection_id, common::RecordId record_id, schema::RecordData data);
    std::expected<void, StorageError> erase(common::CollectionId collection_id, common::RecordId record_id);
    [[nodiscard]] std::expected<StorageCursor, StorageError> scan(common::CollectionId collection_id) const;
    void clear() noexcept;

private:
    StorageEngine() = default;

    struct CollectionState
    {
        schema::CollectionSchema schema;
        std::unique_ptr<StorageStore> store;
    };

    [[nodiscard]] std::filesystem::path store_path(common::CollectionId collection_id) const;
    [[nodiscard]] std::expected<void, StorageError> validate(
        const schema::CollectionSchema & schema, const schema::RecordData & data) const;
    std::filesystem::path data_directory_;
    filesystem::FileSystem * filesystem_ {nullptr};
    std::unordered_map<common::CollectionId, CollectionState> collections_;

    friend class litedb::core::engine::DatabaseInstance;
};

} // namespace litedb::core::storage
