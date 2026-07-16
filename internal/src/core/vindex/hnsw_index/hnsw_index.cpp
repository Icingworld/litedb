#include "core/vindex/hnsw_index/hnsw_index.hpp"

namespace litedb::core::vindex
{

namespace
{

[[nodiscard]]
VectorIndexError not_implemented()
{
    return VectorIndexError {
        VectorIndexErrorCode::UnsupportedIndexKind,
        "HNSW vector index is not implemented",
    };
}

} // namespace

HnswIndex::HnswIndex(HnswIndexOptions options) noexcept
    : options_(options)
{
}

VectorIndexKind HnswIndex::kind() const noexcept
{
    return VectorIndexKind::Hnsw;
}

VectorDistanceMetric HnswIndex::metric() const noexcept
{
    return options_.metric;
}

std::size_t HnswIndex::dimension() const noexcept
{
    return options_.dimension;
}

std::expected<void, VectorIndexError> HnswIndex::insert(const VectorIndexKey &, common::RecordId)
{
    return std::unexpected(not_implemented());
}

std::expected<void, VectorIndexError> HnswIndex::erase(common::RecordId)
{
    return std::unexpected(not_implemented());
}

std::expected<std::vector<VectorSearchResult>, VectorIndexError> HnswIndex::search(
    const VectorIndexKey &,
    VectorSearchRequest
) const
{
    return std::unexpected(not_implemented());
}

std::size_t HnswIndex::size() const noexcept
{
    return 0;
}

} // namespace litedb::core::vindex
