#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <vector>

#include "core/index/btree_index/btree_page_store.hpp"
#include "core/index/scalar_index.hpp"

namespace litedb::core::index
{

/**
 * @brief 基于持久化页面的 B+Tree 索引
 */
class BTreeIndex final
{
public:
    BTreeIndex(const BTreeIndex &) = delete;

    BTreeIndex & operator=(const BTreeIndex &) = delete;

    BTreeIndex(BTreeIndex &&) noexcept = default;

    BTreeIndex & operator=(BTreeIndex &&) noexcept = default;

private:
    explicit BTreeIndex(btree_index::BTreePageStore store) noexcept;

public:
    [[nodiscard]]
    static std::expected<BTreeIndex, btree_index::BTreePageStoreError> create(
        std::filesystem::path path,
        common::IndexId index_id,
        common::LogicalType key_type,
        filesystem::FileSystem & filesystem
    );

    [[nodiscard]]
    static std::expected<BTreeIndex, btree_index::BTreePageStoreError> open(
        std::filesystem::path path,
        common::IndexId expected_index_id,
        common::LogicalType expected_key_type,
        filesystem::FileSystem & filesystem
    );

    /**
     * @brief 获取逻辑索引类型
     * @note 签名与 ScalarIndex 保持一致，待实现完成后添加 override。
     */
    [[nodiscard]]
    IndexKind kind() const noexcept;

    /**
     * @brief 插入完整的索引条目
     * @note 当前仅建立接口框架，树插入与分裂尚未实现。
     */
    std::expected<void, IndexError> insert(
        const ScalarIndexKey & key,
        common::RecordId record_id
    );

    /**
     * @brief 删除完整的索引条目
     * @note 当前仅建立接口框架，树删除与合并尚未实现。
     */
    std::expected<void, IndexError> erase(
        const ScalarIndexKey & key,
        common::RecordId record_id
    );

    /**
     * @brief 查找等于给定标量键的全部记录 ID
     * @note 当前仅建立接口框架，root-to-leaf 查找尚未实现。
     */
    [[nodiscard]]
    std::expected<std::vector<common::RecordId>, IndexError> find_equal(
        const ScalarIndexKey & key
    ) const;

    /**
     * @brief 扫描给定范围内的全部记录 ID
     * @note 当前仅建立接口框架，叶子链扫描尚未实现。
     */
    [[nodiscard]]
    std::expected<std::vector<common::RecordId>, IndexError> scan_range(
        const IndexRange & range
    ) const;

    /**
     * @brief 获取索引条目数量
     * @note 签名与 ScalarIndex 保持一致，待实现完成后添加 override。
     */
    [[nodiscard]]
    std::size_t size() const noexcept;

    /**
     * @brief 获取索引文件路径
     * @return 索引文件路径
     */
    [[nodiscard]]
    const std::filesystem::path & path() const noexcept;

    /**
     * @brief 获取索引 ID
     * @return 索引 ID
     */
    [[nodiscard]]
    common::IndexId index_id() const noexcept;

    /**
     * @brief 获取键类型
     * @return 键类型
     */
    [[nodiscard]]
    const common::LogicalType & key_type() const noexcept;

    /**
     * @brief 获取根页面 ID
     * @return 根页面 ID
     */
    [[nodiscard]]
    btree_index::BTreePageId root_page_id() const noexcept;

    /**
     * @brief 获取页面数量
     * @return 页面数量
     */
    [[nodiscard]]
    std::uint64_t page_count() const noexcept;

    /**
     * @brief 获取索引条目数量
     * @return 索引条目数量
     */
    [[nodiscard]]
    std::uint64_t entry_count() const noexcept;

private:
    btree_index::BTreePageStore store_;         ///< 页面存储
};

} // namespace litedb::core::index
