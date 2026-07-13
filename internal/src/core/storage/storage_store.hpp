#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <unordered_map>

#include "core/common/ids.hpp"
#include "core/filesystem/file_handle.hpp"
#include "core/filesystem/filesystem.hpp"
#include "core/schema/record.hpp"
#include "core/storage/storage_cursor.hpp"
#include "core/storage/storage_error.hpp"

namespace litedb::core::storage
{

/**
 * @brief 物理记录 ID
 */
struct PhysicalRid
{
    std::uint32_t page_id;         ///< 页 ID
    std::uint16_t slot_id;         ///< 槽 ID
};

/**
 * @brief 单个集合的持久化存储
 */
class StorageStore final
{
private:
    StorageStore(common::CollectionId collection_id, filesystem::FileHandle file) noexcept;

public:
    static constexpr std::uint32_t PageSize = 4096;

    /**
     * @brief 创建持久化存储器
     * @param path 路径
     * @param collection_id 集合 ID
     * @param filesystem 文件系统
     * @return 持久化存储器
     */
    static std::expected<std::unique_ptr<StorageStore>, StorageStoreError> create(
        std::filesystem::path path,
        common::CollectionId collection_id,
        filesystem::FileSystem & filesystem
    );

    /**
     * @brief 打开持久化存储器
     * @param path 路径
     * @param collection_id 集合 ID
     * @param filesystem 文件系统
     * @return 持久化存储器
     */
    static std::expected<std::unique_ptr<StorageStore>, StorageStoreError> open(
        std::filesystem::path path,
        common::CollectionId collection_id,
        filesystem::FileSystem & filesystem
    );

    /**
     * @brief 获取记录
     * @param id 记录 ID
     * @return 记录
     */
    [[nodiscard]]
    std::expected<schema::Record, StorageStoreError> get(common::RecordId id) const;

    /**
     * @brief 插入记录
     * @param data 记录数据
     * @return 记录 ID
     */
    std::expected<common::RecordId, StorageStoreError> insert(schema::RecordData data);

    /**
     * @brief 更新记录
     * @param id 记录 ID
     * @param data 记录数据
     * @return 是否成功
     */
    std::expected<void, StorageStoreError> update(common::RecordId id, schema::RecordData data);

    /**
     * @brief 删除记录
     * @param id 记录 ID
     * @return 是否成功
     */
    std::expected<void, StorageStoreError> erase(common::RecordId id);

    /**
     * @brief 扫描记录
     * @return 记录游标
     */
    [[nodiscard]]
    StorageCursor scan() const;

private:
    /**
     * @brief 初始化持久化存储器
     * @return 是否成功
     */
    std::expected<void, StorageStoreError> initialize();

    /**
     * @brief 加载持久化存储器
     * @return 是否成功
     */
    std::expected<void, StorageStoreError> load();

    /**
     * @brief 写入头信息
     * @return 是否成功
     */
    std::expected<void, StorageStoreError> write_header();

    /**
     * @brief 放置记录
     * @param id 记录 ID
     * @param data 记录数据
     * @return 物理记录 ID
     */
    std::expected<PhysicalRid, StorageStoreError> place(
        common::RecordId id,
        const schema::RecordData & data
    );

    /**
     * @brief 读取记录
     * @param rid 物理记录 ID
     * @return 记录
     */
    std::expected<schema::Record, StorageStoreError> read(PhysicalRid rid) const;

    /**
     * @brief 标记记录为删除
     * @param rid 物理记录 ID
     * @return 是否成功
     */
    std::expected<void, StorageStoreError> mark_deleted(PhysicalRid rid);

private:
    common::CollectionId collection_id_;        ///< 集合 ID
    mutable filesystem::FileHandle file_;       ///< 文件句柄
    common::RecordId next_record_id_ {1};       ///< 下一个记录 ID
    std::uint32_t page_count_ {0};              ///< 页数
    std::unordered_map<common::RecordId, PhysicalRid> locations_;    ///< 记录位置
};

} // namespace litedb::core::storage
