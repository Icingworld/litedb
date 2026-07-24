#pragma once

#include <cstddef>
#include <expected>
#include <filesystem>
#include <vector>

#include "core/common/ids.hpp"
#include "core/filesystem/filesystem.hpp"
#include "core/vindex/hnsw_index/hnsw_store.hpp"
#include "core/vindex/vector_index.hpp"

namespace litedb::core::vindex
{

struct HnswIndexOptions
{
    std::size_t dimension {0};
    VectorDistanceMetric metric {VectorDistanceMetric::L2};
    std::size_t max_neighbors {16};
    std::size_t ef_construction {200};
    std::size_t ef_search_default {64};
    std::size_t random_seed {0};
};

class HnswIndex final : public VectorIndex
{
    friend class VectorIndexEngine;

private:
    explicit HnswIndex(hnsw_index::HnswStore store) noexcept;

public:
    HnswIndex(const HnswIndex &) = delete;
    HnswIndex & operator=(const HnswIndex &) = delete;
    HnswIndex(HnswIndex &&) noexcept = default;
    HnswIndex & operator=(HnswIndex &&) noexcept = default;

    [[nodiscard]]
    static std::expected<HnswIndex, VectorIndexError> create(
        std::filesystem::path path,
        common::VIndexId index_id,
        common::CollectionId collection_id,
        common::ColumnId column_id,
        HnswIndexOptions options,
        filesystem::FileSystem & filesystem
    );

    [[nodiscard]]
    static std::expected<HnswIndex, VectorIndexError> open(
        std::filesystem::path path,
        common::VIndexId expected_index_id,
        common::CollectionId expected_collection_id,
        common::ColumnId expected_column_id,
        HnswIndexOptions expected_options,
        filesystem::FileSystem & filesystem
    );

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

    [[nodiscard]]
    const std::filesystem::path & path() const noexcept;

    [[nodiscard]]
    const HnswIndexOptions & options() const noexcept;

    [[nodiscard]]
    hnsw_index::HnswStoreStats stats() const noexcept;

    [[nodiscard]]
    std::expected<void, VectorIndexError> close();

private:
    [[nodiscard]]
    std::expected<void, VectorIndexError> validate_key(const VectorIndexKey & key) const;

    [[nodiscard]]
    bool matches_record(common::RecordId record_id, const VectorIndexKey & key) const noexcept;

    [[nodiscard]]
    std::size_t random_level(hnsw_index::HnswNodeId node_id) const noexcept;

private:
    HnswIndexOptions options_;
    hnsw_index::HnswStore store_;
};

} // namespace litedb::core::vindex
