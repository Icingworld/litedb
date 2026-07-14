#include "core/index/btree_index.hpp"

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

BTreeIndex::BTreeIndex()
    : buckets_()
    , entry_count_(0)
{
}

IndexKind BTreeIndex::kind() const noexcept
{
    return IndexKind::BTree;
}

std::expected<void, IndexError> BTreeIndex::insert(
    const ScalarIndexKey & key,
    common::RecordId record_id
)
{
    auto & records = buckets_[key];
    if (std::find(records.begin(), records.end(), record_id) != records.end()) [[unlikely]] {
        return std::unexpected(make_index_error(
            IndexErrorCode::DuplicateEntry,
            "Index entry already exists"
        ));
    }
    records.push_back(record_id);
    // 更新键值对数量
    ++entry_count_;
    return {};
}

std::expected<void, IndexError> BTreeIndex::erase(
    const ScalarIndexKey & key,
    common::RecordId record_id
)
{
    // 查找键
    auto bucket = buckets_.find(key);
    if (bucket == buckets_.end()) {
        return std::unexpected(make_index_error(IndexErrorCode::KeyNotFound, "Index key not found"));
    }

    // 在列表中查找记录 ID
    auto & records = bucket->second;
    const auto record = std::find(records.begin(), records.end(), record_id);
    if (record == records.end()) {
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

std::expected<std::vector<common::RecordId>, IndexError> BTreeIndex::find_equal(
    const ScalarIndexKey & key
) const
{
    const auto bucket = buckets_.find(key);
    if (bucket == buckets_.end()) {
        return std::vector<common::RecordId> {};
    }
    return bucket->second;
}

std::expected<std::vector<common::RecordId>, IndexError> BTreeIndex::scan_range(
    const IndexRange & range
) const
{
    // 同时存在上下界时，先判断区间是否为空
    if (range.lower().has_value() && range.upper().has_value()) {
        const auto compared = compare_scalar_index_keys(range.lower()->key, range.upper()->key);
        // 下界大于上界，例如 (10, 5)
        if (compared == std::strong_ordering::greater) {
            return std::vector<common::RecordId> {};
        }
        // 上下界相等但至少有一侧开区间，例如 (5, 5) 或 [5, 5)
        if (compared == std::strong_ordering::equal && (!range.lower()->inclusive || !range.upper()->inclusive)) {
            return std::vector<common::RecordId> {};
        }
    }

    // 确定扫描起点：[begin, end) 左闭右开
    auto begin = buckets_.begin();
    if (range.lower().has_value()) {
        // 闭下界 [L, ...): lower_bound 指向第一个 >= L 的键
        // 开下界 (L, ...): upper_bound 指向第一个 > L 的键
        begin = range.lower()->inclusive ? buckets_.lower_bound(range.lower()->key) : buckets_.upper_bound(range.lower()->key);
    }

    auto end = buckets_.end();
    if (range.upper().has_value()) {
        // 闭上界 ..., U]: upper_bound 指向第一个 > U 的键
        // 开上界 ..., U): lower_bound 指向第一个 >= U 的键
        end = range.upper()->inclusive ? buckets_.upper_bound(range.upper()->key) : buckets_.lower_bound(range.upper()->key);
    }

    // 合并区间内每个键对应的 record id 列表
    std::vector<common::RecordId> records;
    for (auto it = begin; it != end; ++it) {
        records.insert(records.end(), it->second.begin(), it->second.end());
    }
    return records;
}

void BTreeIndex::clear() noexcept
{
    // 清空桶
    buckets_.clear();
    // 更新键值对数量
    entry_count_ = 0;
}

std::size_t BTreeIndex::size() const noexcept
{
    return entry_count_;
}

} // namespace litedb::core::index
