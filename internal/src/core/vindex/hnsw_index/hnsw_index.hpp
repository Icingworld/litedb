#pragma once

#include <cstddef>
#include <expected>
#include <vector>

#include "core/common/ids.hpp"
#include "core/vindex/vector_index.hpp"

namespace litedb::core::vindex
{

/**
 * @brief HNSW 向量索引配置占位
 */
struct HnswIndexOptions
{
    std::size_t dimension {0};
    VectorDistanceMetric metric {VectorDistanceMetric::L2};
    std::size_t max_neighbors {16};
    std::size_t ef_construction {200};
    std::size_t random_seed {0};
};

/**
 * @brief HNSW 向量索引占位实现
 * @details 当前不接入构建和索引工厂，后续在该目录内完成真实实现。
 */
class HnswIndex final : public VectorIndex
{
public:
    explicit HnswIndex(HnswIndexOptions options) noexcept;

public:
    [[nodiscard]]
    VectorIndexKind kind() const noexcept override;

    [[nodiscard]]
    VectorDistanceMetric metric() const noexcept override;

    [[nodiscard]]
    std::size_t dimension() const noexcept override;

    std::expected<void, VectorIndexError> insert(
        const VectorIndexKey & key,
        common::RecordId record_id
    ) override;

    std::expected<void, VectorIndexError> erase(common::RecordId record_id) override;

    [[nodiscard]]
    std::expected<std::vector<VectorSearchResult>, VectorIndexError> search(
        const VectorIndexKey & query,
        VectorSearchRequest request
    ) const override;

    [[nodiscard]]
    std::size_t size() const noexcept override;

private:
    HnswIndexOptions options_;
};

} // namespace litedb::core::vindex
