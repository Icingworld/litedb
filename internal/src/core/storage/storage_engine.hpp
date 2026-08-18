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

namespace litedb::core::storage
{

enum class StorageOpenMode : std::uint8_t
{
    LiveReadOnly,
    TransactionalStaging,
};

/**
 * @brief 存储引擎
 * @details 管理多个集合的持久化存储，提供记录读写与集合生命周期操作
 */
class StorageEngine
{
public:
    StorageEngine(
        std::filesystem::path data_directory,
        filesystem::FileSystem & filesystem,
        StorageOpenMode mode = StorageOpenMode::LiveReadOnly
    ) noexcept;

    StorageEngine(const StorageEngine &) = delete;

    StorageEngine & operator=(const StorageEngine &) = delete;

    StorageEngine(StorageEngine &&) noexcept;

    StorageEngine & operator=(StorageEngine &&) noexcept;

    ~StorageEngine();

public:
    /**
     * @brief 创建集合并初始化存储文件
     * @param schema 集合模式
     * @return 结果
     */
    std::expected<void, StorageError> create_collection(schema::CollectionSchema schema);

    /**
     * @brief 打开已存在的集合存储
     * @param schema 集合模式
     * @return 结果
     */
    std::expected<void, StorageError> open_collection(schema::CollectionSchema schema);

    /**
     * @brief 从正式存储文件重新加载一个已存在的集合
     * @details 新存储成功打开后才替换当前运行时状态
     */
    std::expected<void, StorageError> reload_collection(schema::CollectionSchema schema);

    /**
     * @brief 删除集合及其存储文件
     * @param collection_id 集合 ID
     * @return 结果
     */
    std::expected<void, StorageError> drop_collection(common::CollectionId collection_id);

    /**
     * @brief 判断集合是否已加载
     * @param collection_id 集合 ID
     * @return 是否已加载
     */
    [[nodiscard]]
    bool contains_collection(common::CollectionId collection_id) const noexcept;

    /**
     * @brief 按记录 ID 读取记录
     * @param collection_id 集合 ID
     * @param record_id 记录 ID
     * @return 记录
     */
    [[nodiscard]]
    std::expected<common::Record, StorageError> get(
        common::CollectionId collection_id,
        common::RecordId record_id
    ) const;

    /**
     * @brief 插入记录
     * @param collection_id 集合 ID
     * @param data 记录数据
     * @return 新记录 ID
     */
    std::expected<common::RecordId, StorageError> insert(
        common::CollectionId collection_id,
        common::RecordData data
    );

    /**
     * @brief 更新记录
     * @param collection_id 集合 ID
     * @param record_id 记录 ID
     * @param data 记录数据
     * @return 结果
     */
    std::expected<void, StorageError> update(
        common::CollectionId collection_id,
        common::RecordId record_id,
        common::RecordData data
    );

    /**
     * @brief 删除记录
     * @param collection_id 集合 ID
     * @param record_id 记录 ID
     * @return 结果
     */
    std::expected<void, StorageError> erase(
        common::CollectionId collection_id,
        common::RecordId record_id
    );

    /**
     * @brief 扫描集合中的全部记录
     * @param collection_id 集合 ID
     * @return 记录游标
     */
    [[nodiscard]]
    std::expected<StorageCursor, StorageError> scan(common::CollectionId collection_id) const;

    /**
     * @brief 清空内存中的全部集合状态
     */
    void clear() noexcept;

private:
    /**
     * @brief 集合运行时状态
     */
    struct CollectionState
    {
        schema::CollectionSchema schema;            // 集合模式
        std::unique_ptr<StorageStore> store;        // 持久化存储
    };

    /**
     * @brief 获取集合存储文件路径
     * @param collection_id 集合 ID
     * @return 存储文件路径
     */
    [[nodiscard]]
    std::filesystem::path store_path(common::CollectionId collection_id) const;

    /**
     * @brief 校验记录数据是否符合集合模式
     * @param schema 集合模式
     * @param data 记录数据
     * @return 结果
     */
    [[nodiscard]]
    std::expected<void, StorageError> validate(
        const schema::CollectionSchema & schema,
        const common::RecordData & data
    ) const;

private:
    std::filesystem::path data_directory_;                                      // 数据目录
    filesystem::FileSystem * filesystem_ {nullptr};                             // 文件系统
    std::unordered_map<common::CollectionId, CollectionState> collections_;     // 已加载集合
    StorageOpenMode mode_ {StorageOpenMode::LiveReadOnly};                     // 打开模式
};

} // namespace litedb::core::storage
