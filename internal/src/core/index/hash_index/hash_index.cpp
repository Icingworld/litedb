#include "core/index/hash_index/hash_index.hpp"

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

std::expected<void, IndexError> HashIndex::insert(
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

std::size_t HashIndex::size() const noexcept
{
    return entry_count_;
}

} // namespace litedb::core::index
