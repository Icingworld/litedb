#pragma once

#include <cstddef>
#include <expected>
#include <memory>
#include <vector>

#include "core/common/ids.hpp"
#include "core/vindex/vector_index.hpp"

namespace litedb::core::vindex
{

/**
 * @brief 单个运行时向量索引的描述符
 */
struct VectorIndexDescriptor
{
    common::VIndexId index_id;
    common::CollectionId collection_id;
    common::ColumnId column_id;
    std::size_t column_ordinal {0};
    std::size_t dimension {0};
    VectorIndexKind kind {VectorIndexKind::Flat};
    VectorDistanceMetric metric {VectorDistanceMetric::L2};
    std::size_t max_neighbors {16};
    std::size_t ef_construction {200};
    std::size_t ef_search_default {64};
    std::size_t random_seed {0};
};

/**
 * @brief 单个向量索引运行时实例
 * @details 持有稳定的运行时描述以及具体算法 backend，不承担多索引生命周期策略。
 */
class VectorIndexStore final
{
public:
    VectorIndexStore(VectorIndexDescriptor descriptor, std::unique_ptr<VectorIndex> backend) noexcept;

    VectorIndexStore(const VectorIndexStore &) = delete;
    VectorIndexStore & operator=(const VectorIndexStore &) = delete;
    VectorIndexStore(VectorIndexStore &&) noexcept = default;
    VectorIndexStore & operator=(VectorIndexStore &&) noexcept = default;

    [[nodiscard]] const VectorIndexDescriptor & descriptor() const noexcept;
    [[nodiscard]] std::expected<void, VectorIndexError> insert(const VectorIndexKey & key, common::RecordId record_id);
    [[nodiscard]] std::expected<void, VectorIndexError> erase(common::RecordId record_id);
    [[nodiscard]] std::expected<std::vector<VectorSearchResult>, VectorIndexError> search(
        const VectorIndexKey & query,
        VectorSearchRequest request
    ) const;
    [[nodiscard]] std::size_t size() const noexcept;

private:
    friend class VectorIndexEngine;

    [[nodiscard]] VectorIndex & backend() noexcept;
    [[nodiscard]] const VectorIndex & backend() const noexcept;

private:
    VectorIndexDescriptor descriptor_;
    std::unique_ptr<VectorIndex> backend_;
};

} // namespace litedb::core::vindex
