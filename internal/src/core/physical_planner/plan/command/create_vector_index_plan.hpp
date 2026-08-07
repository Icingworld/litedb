#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include "core/common/ids.hpp"
#include "core/meta/entry/vector_index_entry.hpp"
#include "core/physical_planner/plan/physical_plan.hpp"

namespace litedb::core::physical_planner::plan
{

/**
 * @brief CREATE VINDEX 语句计划
 */
class CreateVectorIndexPlan final : public PhysicalPlan
{
public:
    CreateVectorIndexPlan(
        common::ColumnId column_id,
        std::optional<std::string> index_name,
        meta::entry::VectorIndexKind index_kind,
        meta::entry::VectorDistanceMetric metric,
        std::size_t max_neighbors,
        std::size_t ef_construction,
        std::size_t ef_search_default,
        std::size_t random_seed
    );

public:
    /**
     * @brief 获取列 ID
     * @return 列 ID
     */
    [[nodiscard]]
    common::ColumnId column_id() const noexcept;

    /**
     * @brief 获取索引名称
     * @return 索引名称
     */
    [[nodiscard]]
    const std::optional<std::string> & index_name() const noexcept;

    /**
     * @brief 获取索引类型
     * @return 索引类型
     */
    [[nodiscard]]
    meta::entry::VectorIndexKind index_kind() const noexcept;

    /**
     * @brief 获取距离度量
     * @return 距离度量
     */
    [[nodiscard]]
    meta::entry::VectorDistanceMetric metric() const noexcept;

    /**
     * @brief 获取最大邻居数
     * @return 最大邻居数
     */
    [[nodiscard]]
    std::size_t max_neighbors() const noexcept;

    /**
     * @brief 获取构建时 EF 值
     * @return 构建时 EF 值
     */
    [[nodiscard]]
    std::size_t ef_construction() const noexcept;

    /**
     * @brief 获取搜索时默认 EF 值
     * @return 搜索时默认 EF 值
     */
    [[nodiscard]]
    std::size_t ef_search_default() const noexcept;

    /**
     * @brief 获取随机种子
     * @return 随机种子
     */
    [[nodiscard]]
    std::size_t random_seed() const noexcept;

private:
    common::ColumnId column_id_;                        // 列 ID
    std::optional<std::string> index_name_;             // 索引名称
    meta::entry::VectorIndexKind index_kind_;           // 索引类型
    meta::entry::VectorDistanceMetric metric_;          // 距离度量
    std::size_t max_neighbors_;                         // 最大邻居数
    std::size_t ef_construction_;                       // 构建时 EF 值
    std::size_t ef_search_default_;                     // 搜索时默认 EF 值
    std::size_t random_seed_;                           // 随机种子
};

} // namespace litedb::core::physical_planner::plan