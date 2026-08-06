#pragma once

#include <optional>

#include "core/common/ids.hpp"
#include "core/index/scalar_index_key.hpp"
#include "core/physical_planner/operator/physical_operator.hpp"

namespace litedb::core::physical_planner::op
{

/**
 * @brief 索引查找类型
 */
enum class IndexLookupKind
{
    Equal,              ///< 等值查找
    Range,              ///< 范围查找
};

/**
 * @brief 索引边界
 */
struct IndexBound
{
    index::ScalarIndexKey key;                      ///< 边界键
    bool inclusive {true};                          ///< 是否包含边界
};

/**
 * @brief 索引查找条件
 */
struct IndexLookup
{
    IndexLookupKind kind {IndexLookupKind::Equal};   ///< 查找类型
    std::optional<IndexBound> lower;                 ///< 下界
    std::optional<IndexBound> upper;                 ///< 上界
};

/**
 * @brief 索引扫描算子
 */
class IndexScanOperator final : public PhysicalOperator
{
public:
    IndexScanOperator(
        common::CollectionId collection_id,
        common::IndexId index_id,
        IndexLookup lookup
    ) noexcept;

public:
    /**
     * @brief 获取集合 ID
     * @return 集合 ID
     */
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    /**
     * @brief 获取索引 ID
     * @return 索引 ID
     */
    [[nodiscard]]
    common::IndexId index_id() const noexcept;

    /**
     * @brief 获取查找条件
     * @return 查找条件
     */
    [[nodiscard]]
    const IndexLookup & lookup() const noexcept;

private:
    common::CollectionId collection_id_;            ///< 集合 ID
    common::IndexId index_id_;                      ///< 索引 ID
    IndexLookup lookup_;                            ///< 查找条件
};

} // namespace litedb::core::physical_planner::op
