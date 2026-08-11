#pragma once

#include <optional>

#include "core/common/ids.hpp"
#include "core/index/scalar_index_key.hpp"
#include "core/physical_planner/operator/physical_operator.hpp"

namespace litedb::core::physical_planner::op
{

// 索引查找类型
enum class IndexLookupKind
{
    Equal,              // 等值查找
    Range,              // 范围查找
};

// 索引边界
struct IndexBound
{
    index::ScalarIndexKey key;                      // 边界键
    bool inclusive {true};                          // 是否包含边界
};

// 索引查找条件
struct IndexLookup
{
    IndexLookupKind kind {IndexLookupKind::Equal};
    std::optional<IndexBound> lower; // 下界，在等值查找情况下，还承担等值键的作用
    std::optional<IndexBound> upper; // 上界
};

// 索引扫描算子
class IndexScanOperator final : public PhysicalOperator
{
public:
    IndexScanOperator(
        common::CollectionId collection_id,
        common::IndexId index_id,
        IndexLookup lookup
    ) noexcept;

public:
    // 获取集合 ID
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    // 获取索引 ID
    [[nodiscard]]
    common::IndexId index_id() const noexcept;

    // 获取查找条件
    [[nodiscard]]
    const IndexLookup & lookup() const noexcept;

private:
    common::CollectionId collection_id_;
    common::IndexId index_id_;
    IndexLookup lookup_;
};

} // namespace litedb::core::physical_planner::op
