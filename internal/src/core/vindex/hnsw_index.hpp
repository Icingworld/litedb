#pragma once

#include <cstddef>
#include <expected>
#include <unordered_map>
#include <vector>

#include "core/common/ids.hpp"
#include "core/schema/value.hpp"
#include "core/vindex/vector_index.hpp"

namespace litedb::core::vindex
{

/**
 * @brief HNSW 索引配置
 */
struct HnswIndexOptions
{
    std::size_t dimension {0};                                      ///< 向量维度
    VectorDistanceMetric metric {VectorDistanceMetric::L2};         ///< 距离度量
    std::size_t max_neighbors {16};                                 ///< 每层最大邻居数量
    std::size_t ef_construction {200};                              ///< 构建候选数量
    std::size_t random_seed {0};                                    ///< 随机种子
};

/**
 * @brief HNSW 向量索引
 */
class HnswIndex final : public VectorIndex
{
public:
    explicit HnswIndex(HnswIndexOptions options);

public:
    [[nodiscard]]
    VectorIndexKind kind() const noexcept override;

    [[nodiscard]]
    VectorDistanceMetric metric() const noexcept override;

    [[nodiscard]]
    std::size_t dimension() const noexcept override;

    std::expected<void, VectorIndexError> insert(
        const schema::VectorValue & vector,
        common::RecordId record_id
    ) override;

    std::expected<void, VectorIndexError> erase(common::RecordId record_id) override;

    std::expected<void, VectorIndexError> update(
        const schema::VectorValue & vector,
        common::RecordId record_id
    ) override;

    [[nodiscard]]
    std::expected<std::vector<VectorSearchResult>, VectorIndexError> search(
        const schema::VectorValue & query,
        VectorSearchParameters parameters
    ) const override;

    void clear() noexcept override;

    [[nodiscard]]
    std::size_t size() const noexcept override;

    [[nodiscard]]
    const HnswIndexOptions & options() const noexcept;

private:
    [[nodiscard]]
    std::expected<void, VectorIndexError> validate_vector(const schema::VectorValue & vector) const;

private:
    HnswIndexOptions options_;   ///< HNSW 配置
    std::unordered_map<common::RecordId, schema::VectorValue> vectors_; ///< 记录向量
};

} // namespace litedb::core::vindex
