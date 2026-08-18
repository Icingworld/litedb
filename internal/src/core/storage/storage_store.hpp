#pragma once

#include <expected>
#include <memory>
#include <map>
#include <set>

#include "core/common/ids.hpp"
#include "core/filesystem/filesystem.hpp"
#include "core/common/record.hpp"
#include "core/storage/storage_error.hpp"

namespace litedb::core::storage
{

class StorageStore
{
private:
    StorageStore(
        std::filesystem::path path,
        common::CollectionId collection_id,
        filesystem::FileHandle file
    ) noexcept;

public:
    // 创建并打开指定集合的存储空间
    [[nodiscard]]
    static std::expected<std::unique_ptr<StorageStore>, StorageError> create(
        std::filesystem::path path,
        filesystem::FileSystem & filesystem,
        common::CollectionId collection_id
    );

    // 打开指定集合的存储空间
    [[nodiscard]]
    static std::expected<std::unique_ptr<StorageStore>, StorageError> open(
        std::filesystem::path path,
        filesystem::FileSystem & filesystem,
        common::CollectionId collection_id
    );

    // 获取指定记录
    [[nodiscard]]
    std::expected<common::Record, StorageError> get(common::RecordId record_id) const;

    // 插入记录
    [[nodiscard]]
    std::expected<common::RecordId, StorageError> insert(common::RecordData data);

    // 更新记录
    [[nodiscard]]
    std::expected<void, StorageError> update(common::RecordId record_id, common::RecordData data);

    // 删除记录
    [[nodiscard]]
    std::expected<void, StorageError> erase(common::RecordId record_id);

private:
    // 初始化存储空间
    std::expected<void, StorageError> initialize();

    // 写入存储头
    std::expected<void, StorageError> write_header();

    // 加载存储空间
    std::expected<void, StorageError> load();

private:
    // 物理记录 ID
    struct PhysicalRid
    {
        std::uint32_t page_id; // 页 ID
        std::uint16_t slot_id; // 槽 ID
    };

    // 页空间摘要
    struct PageSpaceSummary
    {
        std::size_t contiguous {0}; // 连续空间大小
        std::size_t reclaimable {0}; // 可回收空间大小
        bool has_deleted_slot {false}; // 是否存在删除的槽
    };

private:
    std::filesystem::path path_;
    common::CollectionId collection_id_;
    // const 接口底层 read_at 不是 const，可能会修改状态
    // 为保持接口对外 const，对内可变，使用 mutable 修饰
    mutable filesystem::FileHandle file_;

    // 以下两个成员变量将会持久化到文件头中
    common::RecordId next_record_id_;
    std::uint32_t page_count_;

    // 用于快速定位记录的物理位置
    std::map<common::RecordId, PhysicalRid> locations_;
    // 储存每一个页的空间摘要
    std::vector<PageSpaceSummary> page_space_summaries_;
    // 用于快速定位空闲空间
    std::set<std::pair<std::size_t, std::uint32_t>> free_space_index_; // <可回收空间大小, 页 ID>
};

} // namespace litedb::core::storage
