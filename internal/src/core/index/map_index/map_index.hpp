#pragma once

#include <map>
#include <vector>

#include "core/index/scalar_index.hpp"

namespace litedb::core::index
{

/**
 * @brief 基于 std::map 的临时有序索引实现
 * @details 作为逻辑 BTree 索引的兼容后端，仅用于页式 BTreeIndex 完成前的过渡。
 * @todo 页式 BTreeIndex 接入 IndexEngine 后删除该实现。
 */
class MapIndex final : public OrderedScalarIndex
{
public:
    MapIndex();

public:
    [[nodiscard]]
    IndexKind kind() const noexcept override;

    std::expected<void, IndexError> insert(
        const ScalarIndexKey & key,
        common::RecordId record_id
    ) override;

    std::expected<void, IndexError> erase(
        const ScalarIndexKey & key,
        common::RecordId record_id
    ) override;

    [[nodiscard]]
    std::expected<std::vector<common::RecordId>, IndexError> find_equal(
        const ScalarIndexKey & key
    ) const override;

    [[nodiscard]]
    std::expected<std::vector<common::RecordId>, IndexError> scan_range(
        const IndexRange & range
    ) const override;

    void clear() noexcept override;

    [[nodiscard]]
    std::size_t size() const noexcept override;

private:
    std::map<
        ScalarIndexKey, std::vector<common::RecordId>, ScalarIndexLess
    > buckets_;
    std::size_t entry_count_;
};

} // namespace litedb::core::index
