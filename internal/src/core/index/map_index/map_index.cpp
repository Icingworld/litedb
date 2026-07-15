#include "core/index/map_index/map_index.hpp"

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

MapIndex::MapIndex()
    : buckets_()
    , entry_count_(0)
{
}

IndexKind MapIndex::kind() const noexcept
{
    return IndexKind::BTree;
}

std::expected<void, IndexError> MapIndex::insert(
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
    ++entry_count_;
    return {};
}

std::expected<void, IndexError> MapIndex::erase(
    const ScalarIndexKey & key,
    common::RecordId record_id
)
{
    auto bucket = buckets_.find(key);
    if (bucket == buckets_.end()) {
        return std::unexpected(make_index_error(IndexErrorCode::KeyNotFound, "Index key not found"));
    }

    auto & records = bucket->second;
    const auto record = std::find(records.begin(), records.end(), record_id);
    if (record == records.end()) {
        return std::unexpected(make_index_error(IndexErrorCode::RecordNotFound, "Record id not found for index key"));
    }

    records.erase(record);
    --entry_count_;
    if (records.empty()) {
        buckets_.erase(bucket);
    }
    return {};
}

std::expected<std::vector<common::RecordId>, IndexError> MapIndex::find_equal(
    const ScalarIndexKey & key
) const
{
    const auto bucket = buckets_.find(key);
    if (bucket == buckets_.end()) {
        return std::vector<common::RecordId> {};
    }
    return bucket->second;
}

std::expected<std::vector<common::RecordId>, IndexError> MapIndex::scan_range(
    const IndexRange & range
) const
{
    if (range.lower().has_value() && range.upper().has_value()) {
        const auto compared = compare_scalar_index_keys(range.lower()->key, range.upper()->key);
        if (compared == std::strong_ordering::greater) {
            return std::vector<common::RecordId> {};
        }
        if (compared == std::strong_ordering::equal &&
            (!range.lower()->inclusive || !range.upper()->inclusive)) {
            return std::vector<common::RecordId> {};
        }
    }

    auto begin = buckets_.begin();
    if (range.lower().has_value()) {
        begin = range.lower()->inclusive
            ? buckets_.lower_bound(range.lower()->key)
            : buckets_.upper_bound(range.lower()->key);
    }

    auto end = buckets_.end();
    if (range.upper().has_value()) {
        end = range.upper()->inclusive
            ? buckets_.upper_bound(range.upper()->key)
            : buckets_.lower_bound(range.upper()->key);
    }

    std::vector<common::RecordId> records;
    for (auto it = begin; it != end; ++it) {
        records.insert(records.end(), it->second.begin(), it->second.end());
    }
    return records;
}

void MapIndex::clear() noexcept
{
    buckets_.clear();
    entry_count_ = 0;
}

std::size_t MapIndex::size() const noexcept
{
    return entry_count_;
}

} // namespace litedb::core::index
