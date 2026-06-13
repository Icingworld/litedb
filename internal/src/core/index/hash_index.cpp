#include "core/index/hash_index.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace litedb::core::index
{

namespace
{

[[nodiscard]]
IndexError make_index_error(IndexErrorCode code, std::string message)
{
    return IndexError {code, std::move(message)};
}

} // namespace

HashIndex::HashIndex()
    : buckets_()
    , entry_count_(0)
{
}

IndexKind HashIndex::kind() const noexcept
{
    return IndexKind::Hash;
}

bool HashIndex::supports_range_scan() const noexcept
{
    return false;
}

std::expected<void, IndexError> HashIndex::insert(
    const ScalarIndexKey & key,
    common::RecordId record_id
)
{
    // 插入键值对
    buckets_[key].push_back(record_id);
    // 更新键值对数量
    ++entry_count_;
    return {};
}

std::expected<void, IndexError> HashIndex::erase(
    const ScalarIndexKey & key,
    common::RecordId record_id
)
{
    // 查找键
    auto bucket = buckets_.find(key);
    if (bucket == buckets_.end()) [[unlikely]] {
        return std::unexpected(make_index_error(IndexErrorCode::KeyNotFound, "Index key not found"));
    }

    // 在列表中查找记录 ID
    auto & records = bucket->second;
    const auto record = std::find(records.begin(), records.end(), record_id);
    if (record == records.end()) [[unlikely]] {
        return std::unexpected(make_index_error(IndexErrorCode::RecordNotFound, "Record id not found for index key"));
    }

    // 删除记录 ID
    records.erase(record);
    // 更新键值对数量
    --entry_count_;
    // 如果列表为空，则删除桶
    if (records.empty()) {
        buckets_.erase(bucket);
    }
    return {};
}

std::expected<std::vector<common::RecordId>, IndexError> HashIndex::find_equal(
    const ScalarIndexKey & key
) const
{
    const auto bucket = buckets_.find(key);
    if (bucket == buckets_.end()) {
        return std::vector<common::RecordId> {};
    }
    return bucket->second;
}

std::expected<std::vector<common::RecordId>, IndexError> HashIndex::scan_range(
    const IndexRange &
) const
{
    // 哈希索引不支持范围扫描
    return std::unexpected(make_index_error(
        IndexErrorCode::UnsupportedRangeScan,
        "Hash index does not support range scans"
    ));
}

void HashIndex::clear() noexcept
{
    // 清空桶
    buckets_.clear();
    // 更新键值对数量
    entry_count_ = 0;
}

std::size_t HashIndex::size() const noexcept
{
    return entry_count_;
}

} // namespace litedb::core::index
