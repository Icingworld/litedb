#pragma once

#include <expected>
#include <memory>
#include <unordered_map>

#include "core/schema/collection.hpp"
#include "core/storage/in_memory_collection_storage.hpp"
#include "core/storage/storage_error.hpp"

namespace litedb::core::storage
{

/**
 * @brief 存储管理器
 */
class StorageManager
{
public:
    /**
     * @brief 创建集合
     * @param collection_schema 集合 schema
     * @return 结果
     */
    std::expected<void, StorageError> create_collection(schema::CollectionSchema collection_schema);

    /**
     * @brief 删除集合
     * @param collection_id 集合 ID
     * @return 结果
     */
    std::expected<void, StorageError> drop_collection(common::CollectionId collection_id);

    /**
     * @brief 查找集合
     * @param collection_id 集合 ID
     * @return 集合存储
     */
    [[nodiscard]]
    CollectionStorage * find_collection(common::CollectionId collection_id) noexcept;

    /**
     * @brief 查找集合
     * @param collection_id 集合 ID
     * @return 集合存储
     */
    [[nodiscard]]
    const CollectionStorage * find_collection(common::CollectionId collection_id) const noexcept;

private:
    std::unordered_map<common::CollectionId, std::unique_ptr<InMemoryCollectionStorage>> collections_;  ///< 集合存储
};

} // namespace litedb::core::storage
