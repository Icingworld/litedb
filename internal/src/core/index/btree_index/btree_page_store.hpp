#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>

#include "core/common/ids.hpp"
#include "core/common/logical_type.hpp"
#include "core/filesystem/file_handle.hpp"
#include "core/filesystem/filesystem.hpp"
#include "core/index/btree_index/btree_page.hpp"
#include "core/index/btree_index/btree_page_codec.hpp"

namespace litedb::core::index::btree_index
{

// B+ 树页存储错误码
enum class BTreePageStoreErrorCode : std::uint8_t
{
    FileSystemError,      // 文件系统操作失败
    InvalidFormat,        // 文件格式或文件级元数据无效
    UnsupportedVersion,   // 不支持的文件格式版本
    ChecksumMismatch,     // 文件或页面校验和不匹配
    CorruptedPage,        // 节点页损坏
    PageNotFound,         // 页 ID 不存在
    InvalidPage,          // 待写入页面状态无效
    PageCodecError,       // 单页编解码失败
};

// B+ 树页存储错误
struct BTreePageStoreErrorContext
{
    std::optional<BTreePageCodecErrorCode> codec_code;    // 页编解码错误码
};

using BTreePageStoreError = error::Error;

// 单个 B+ 树索引文件的页面存储
// 负责文件头、PageId 分配和节点页随机读写，不负责树查找、分裂、合并或空闲页回收。
class BTreePageStore final
{
public:
    BTreePageStore(const BTreePageStore &) = delete;

    BTreePageStore & operator=(const BTreePageStore &) = delete;

    BTreePageStore(BTreePageStore &&) noexcept = default;

    BTreePageStore & operator=(BTreePageStore &&) noexcept = default;

private:
    BTreePageStore(
        std::filesystem::path path,
        common::IndexId index_id,
        common::LogicalType key_type,
        filesystem::FileHandle file
    ) noexcept;

public:
    // 物理页头部大小
    // 物理页头部大小等于物理页大小，占据 4096 字节，设计与 storage 层一致。
    static constexpr std::size_t HeaderSize = BTreePageCodec::PageSize;

    // 创建新的 B+ 树索引文件
    [[nodiscard]]
    static std::expected<BTreePageStore, BTreePageStoreError> create(
        std::filesystem::path path,
        common::IndexId index_id,
        common::LogicalType key_type,
        filesystem::FileSystem & filesystem
    );

    // 打开已有 B+ 树索引文件
    [[nodiscard]]
    static std::expected<BTreePageStore, BTreePageStoreError> open(
        std::filesystem::path path,
        common::IndexId expected_index_id,
        common::LogicalType expected_key_type,
        filesystem::FileSystem & filesystem
    );

    // 获取索引文件路径
    [[nodiscard]]
    const std::filesystem::path & path() const noexcept;

    // 获取索引 ID
    [[nodiscard]]
    common::IndexId index_id() const noexcept;

    // 获取索引键类型
    [[nodiscard]]
    const common::LogicalType & key_type() const noexcept;

    // 获取根页 ID
    [[nodiscard]]
    BTreePageId root_page_id() const noexcept;

    // 获取页数量
    [[nodiscard]]
    std::uint64_t page_count() const noexcept;

    [[nodiscard]]
    std::uint64_t free_page_count() const noexcept;

    // 获取索引条目数量
    [[nodiscard]]
    std::uint64_t entry_count() const noexcept;

    // 分配并持久化一个空叶子页
    [[nodiscard]]
    std::expected<BTreeLeafPage, BTreePageStoreError> allocate_leaf_page(
        BTreePageId previous_page_id = InvalidBTreePageId,
        BTreePageId next_page_id = InvalidBTreePageId
    );

    // 分配并持久化一个空内部页
    [[nodiscard]]
    std::expected<BTreeInternalPage, BTreePageStoreError> allocate_internal_page(
        BTreePageId first_child_id
    );

    // 读取节点页
    [[nodiscard]]
    std::expected<BTreePage, BTreePageStoreError> read_page(BTreePageId page_id) const;

    // 覆盖写入已有节点页
    [[nodiscard]]
    std::expected<void, BTreePageStoreError> write_page(const BTreePage & page);

    // 更新根页 ID；空树使用 InvalidBTreePageId
    [[nodiscard]]
    std::expected<void, BTreePageStoreError> set_root_page_id(BTreePageId page_id);

    // 更新索引条目计数
    [[nodiscard]]
    std::expected<void, BTreePageStoreError> set_entry_count(std::uint64_t count);

    // Publish a newly built tree and its entry count in one header write.
    [[nodiscard]]
    std::expected<void, BTreePageStoreError> publish_tree(
        BTreePageId root_page_id,
        std::uint64_t entry_count
    );

    [[nodiscard]]
    std::expected<void, BTreePageStoreError> release_page(BTreePageId page_id);

    // 同步文件数据和读取所需元数据
    [[nodiscard]]
    std::expected<void, BTreePageStoreError> sync_data();

    // 同步文件数据和全部文件元数据
    [[nodiscard]]
    std::expected<void, BTreePageStoreError> sync_all();

private:
    // 初始化文件
    [[nodiscard]]
    std::expected<void, BTreePageStoreError> initialize();

    // 加载文件
    [[nodiscard]]
    std::expected<void, BTreePageStoreError> load(
        common::IndexId expected_index_id,
        const common::LogicalType & expected_key_type
    );

    // 写入文件头
    [[nodiscard]]
    std::expected<void, BTreePageStoreError> write_header();

    // 验证页引用
    [[nodiscard]]
    std::expected<void, BTreePageStoreError> validate_page_references(const BTreePage & page) const;

    // 追加页
    [[nodiscard]]
    std::expected<void, BTreePageStoreError> append_page(const BTreePage & page);

    [[nodiscard]]
    std::expected<std::optional<BTreePageId>, BTreePageStoreError> acquire_free_page();

    [[nodiscard]]
    std::expected<BTreePageId, BTreePageStoreError> read_free_page_next(BTreePageId page_id) const;

    [[nodiscard]]
    std::expected<void, BTreePageStoreError> write_free_page(
        BTreePageId page_id,
        BTreePageId next_free_page_id
    );

    // 获取页偏移量
    [[nodiscard]]
    std::uint64_t page_offset(BTreePageId page_id) const noexcept;

private:
    std::filesystem::path path_;                        // 索引文件路径
    common::IndexId index_id_;                          // 索引 ID
    common::LogicalType key_type_;                      // 索引键类型
    mutable filesystem::FileHandle file_;               // 文件句柄
    BTreePageId root_page_id_ {InvalidBTreePageId};     // 根页 ID
    BTreePageId next_page_id_ {1};                      // 下一个待分配页 ID
    std::uint64_t entry_count_ {0};                     // 索引条目数量
    BTreePageId free_page_head_ {InvalidBTreePageId};   // 空闲页链表头
    std::uint64_t free_page_count_ {0};                 // 空闲页数量
};

} // namespace litedb::core::index::btree_index

namespace litedb::core::error
{
template <>
struct ErrorTraits<index::btree_index::BTreePageStoreErrorCode>
{
    static constexpr ErrorCategory category = ErrorCategory::Index;
};
} // namespace litedb::core::error
