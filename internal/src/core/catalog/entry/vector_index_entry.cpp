#include "core/catalog/entry/vector_index_entry.hpp"

#include <utility>

namespace litedb::core::catalog::entry
{

VectorIndexEntry::VectorIndexEntry(
    common::VIndexId id,
    common::CollectionId collection_id,
    common::ColumnId column_id,
    std::string name,
    VectorIndexKind index_kind,
    VectorDistanceMetric metric,
    std::size_t dimension,
    HnswOptions hnsw_options
)
    : CatalogEntry(CatalogEntryKind::VectorIndex, id, std::move(name))
    , collection_id_(collection_id)
    , column_id_(column_id)
    , index_kind_(index_kind)
    , metric_(metric)
    , dimension_(dimension)
    , hnsw_options_(hnsw_options)
{}

common::VIndexId VectorIndexEntry::id() const noexcept
{
    return raw_id();
}

common::CollectionId VectorIndexEntry::collection_id() const noexcept
{
    return collection_id_;
}

common::ColumnId VectorIndexEntry::column_id() const noexcept
{
    return column_id_;
}

VectorIndexKind VectorIndexEntry::index_kind() const noexcept
{
    return index_kind_;
}

VectorDistanceMetric VectorIndexEntry::metric() const noexcept
{
    return metric_;
}

std::size_t VectorIndexEntry::dimension() const noexcept
{
    return dimension_;
}

const HnswOptions & VectorIndexEntry::hnsw_options() const noexcept
{
    return hnsw_options_;
}

std::size_t VectorIndexEntry::max_neighbors() const noexcept
{
    return hnsw_options_.max_neighbors;
}

std::size_t VectorIndexEntry::ef_construction() const noexcept
{
    return hnsw_options_.ef_construction;
}

std::size_t VectorIndexEntry::ef_search_default() const noexcept
{
    return hnsw_options_.ef_search_default;
}

std::size_t VectorIndexEntry::random_seed() const noexcept
{
    return hnsw_options_.random_seed;
}

} // namespace litedb::core::catalog::entry
