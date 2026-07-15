#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>

#include "core/index/btree_index/btree_page_store.hpp"

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

    [[nodiscard]]
    const std::filesystem::path & path() const noexcept;

    [[nodiscard]]
    common::IndexId index_id() const noexcept;

    [[nodiscard]]
    const common::LogicalType & key_type() const noexcept;

    [[nodiscard]]
    btree_index::BTreePageId root_page_id() const noexcept;

    [[nodiscard]]
    std::uint64_t page_count() const noexcept;

    [[nodiscard]]
    std::uint64_t entry_count() const noexcept;

private:
    explicit BTreeIndex(btree_index::BTreePageStore store) noexcept;

private:
    btree_index::BTreePageStore store_;
};

} // namespace litedb::core::index
