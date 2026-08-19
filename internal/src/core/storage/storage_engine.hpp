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

// 存储打开模式
enum class StorageOpenMode : std::uint8_t
{
    LiveReadOnly,
    TransactionalStaging,
};

// 存储引擎
// 管理多个集合的持久化存储，提供记录读写与集合生命周期操作
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
    // 创建集合并初始化存储文件
    std::expected<void, StorageError> create_collection(schema::CollectionSchema schema);

    // 打开已存在的集合存储
    std::expected<void, StorageError> open_collection(schema::CollectionSchema schema);

    // 从正式存储文件重新加载一个已存在的集合
    // 新存储成功打开后才替换当前运行时状态
    std::expected<void, StorageError> reload_collection(schema::CollectionSchema schema);

    // 删除集合及其存储文件
    std::expected<void, StorageError> drop_collection(common::CollectionId collection_id);

    // 判断集合是否已加载
    [[nodiscard]]
    bool contains_collection(common::CollectionId collection_id) const noexcept;

    // 按记录 ID 读取记录
    [[nodiscard]]
    std::expected<common::Record, StorageError> get(
        common::CollectionId collection_id,
        common::RecordId record_id
    ) const;

    // 插入记录
    std::expected<common::RecordId, StorageError> insert(
        common::CollectionId collection_id,
        common::RecordData data
    );

    // 更新记录
    std::expected<void, StorageError> update(
        common::CollectionId collection_id,
        common::RecordId record_id,
        common::RecordData data
    );

    // 删除记录
    std::expected<void, StorageError> erase(
        common::CollectionId collection_id,
        common::RecordId record_id
    );

    // 扫描集合中的全部记录
    [[nodiscard]]
    std::expected<StorageCursor, StorageError> scan(common::CollectionId collection_id) const;

    // 清空内存中的全部集合状态
    void clear() noexcept;

private:
    // 集合运行时状态
    struct CollectionState
    {
        schema::CollectionSchema schema;
        std::unique_ptr<StorageStore> store;
    };

    // 获取集合存储文件路径
    [[nodiscard]]
    std::filesystem::path store_path(common::CollectionId collection_id) const;

    // 校验记录数据是否符合集合模式
    [[nodiscard]]
    std::expected<void, StorageError> validate(
        const schema::CollectionSchema & schema,
        const common::RecordData & data
    ) const;

private:
    std::filesystem::path data_directory_;
    filesystem::FileSystem * filesystem_ {nullptr};
    std::unordered_map<common::CollectionId, CollectionState> collections_;
    StorageOpenMode mode_ {StorageOpenMode::LiveReadOnly};
};

} // namespace litedb::core::storage
