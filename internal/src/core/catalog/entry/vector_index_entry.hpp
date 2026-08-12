#pragma once

#include <cstddef>
#include <string>

#include "core/catalog/entry/catalog_entry.hpp"
#include "core/common/ids.hpp"

namespace litedb::core::catalog::entry
{

// 向量索引类型
enum class VectorIndexKind
{
    Hnsw,
};

// 向量距离度量
enum class VectorDistanceMetric
{
    L2,
    InnerProduct,
    Cosine,
};

// HNSW 索引参数
struct HnswOptions
{
    std::size_t max_neighbors {16}; // 最大邻居数量
    std::size_t ef_construction {200}; // 构建候选数量
    std::size_t ef_search_default {50}; // 默认搜索候选数量
    std::size_t random_seed {0}; // 随机种子
};

// 向量索引项
class VectorIndexEntry final : public CatalogEntry
{
public:
    VectorIndexEntry(
        common::VIndexId id,
        common::CollectionId collection_id,
        common::ColumnId column_id,
        std::string name,
        VectorIndexKind index_kind,
        VectorDistanceMetric metric,
        std::size_t dimension,
        HnswOptions hnsw_options = {}
    );

public:
    // 获取向量索引 ID
    [[nodiscard]]
    common::VIndexId id() const noexcept;

    // 获取集合 ID
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    // 获取列 ID
    [[nodiscard]]
    common::ColumnId column_id() const noexcept;

    // 获取向量索引类型
    [[nodiscard]]
    VectorIndexKind index_kind() const noexcept;

    // 获取距离度量
    [[nodiscard]]
    VectorDistanceMetric metric() const noexcept;

    // 获取向量维度
    [[nodiscard]]
    std::size_t dimension() const noexcept;

    // 获取 HNSW 参数
    [[nodiscard]]
    const HnswOptions & hnsw_options() const noexcept;

    // 获取最大邻居数量
    [[nodiscard]]
    std::size_t max_neighbors() const noexcept;

    // 获取构建候选数量
    [[nodiscard]]
    std::size_t ef_construction() const noexcept;

    // 获取默认搜索候选数量
    [[nodiscard]]
    std::size_t ef_search_default() const noexcept;

    // 获取随机种子
    [[nodiscard]]
    std::size_t random_seed() const noexcept;

private:
    common::CollectionId collection_id_; // 集合 ID
    common::ColumnId column_id_; // 列 ID
    VectorIndexKind index_kind_; // 向量索引类型
    VectorDistanceMetric metric_; // 距离度量
    std::size_t dimension_; // 向量维度
    HnswOptions hnsw_options_; // HNSW 参数
};

} // namespace litedb::core::catalog::entry
