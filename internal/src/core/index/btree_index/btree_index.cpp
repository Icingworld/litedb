#include "core/index/btree_index/btree_index.hpp"

#include <utility>

namespace litedb::core::index
{

BTreeIndex::BTreeIndex(btree_index::BTreePageStore store) noexcept
    : store_(std::move(store))
{
}

std::expected<BTreeIndex, btree_index::BTreePageStoreError> BTreeIndex::create(
    std::filesystem::path path,
    common::IndexId index_id,
    common::LogicalType key_type,
    filesystem::FileSystem & filesystem
)
{
    auto store = btree_index::BTreePageStore::create(
        std::move(path),
        index_id,
        std::move(key_type),
        filesystem
    );
    if (!store.has_value()) {
        return std::unexpected(std::move(store.error()));
    }
    return BTreeIndex {std::move(store.value())};
}

std::expected<BTreeIndex, btree_index::BTreePageStoreError> BTreeIndex::open(
    std::filesystem::path path,
    common::IndexId expected_index_id,
    common::LogicalType expected_key_type,
    filesystem::FileSystem & filesystem
)
{
    auto store = btree_index::BTreePageStore::open(
        std::move(path),
        expected_index_id,
        std::move(expected_key_type),
        filesystem
    );
    if (!store.has_value()) {
        return std::unexpected(std::move(store.error()));
    }
    return BTreeIndex {std::move(store.value())};
}

const std::filesystem::path & BTreeIndex::path() const noexcept
{
    return store_.path();
}

common::IndexId BTreeIndex::index_id() const noexcept
{
    return store_.index_id();
}

const common::LogicalType & BTreeIndex::key_type() const noexcept
{
    return store_.key_type();
}

btree_index::BTreePageId BTreeIndex::root_page_id() const noexcept
{
    return store_.root_page_id();
}

std::uint64_t BTreeIndex::page_count() const noexcept
{
    return store_.page_count();
}

std::uint64_t BTreeIndex::entry_count() const noexcept
{
    return store_.entry_count();
}

} // namespace litedb::core::index
