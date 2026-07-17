#include "core/vindex/vector_index_store.hpp"

#include <utility>

namespace litedb::core::vindex
{

VectorIndexStore::VectorIndexStore(VectorIndexDescriptor descriptor, std::unique_ptr<VectorIndex> backend) noexcept
    : descriptor_(std::move(descriptor)), backend_(std::move(backend))
{
}

const VectorIndexDescriptor & VectorIndexStore::descriptor() const noexcept
{
    return descriptor_;
}

std::expected<void, VectorIndexError> VectorIndexStore::insert(
    const VectorIndexKey & key,
    common::RecordId record_id
)
{
    return backend_->insert(key, record_id);
}

std::expected<void, VectorIndexError> VectorIndexStore::erase(common::RecordId record_id)
{
    return backend_->erase(record_id);
}

std::expected<std::vector<VectorSearchResult>, VectorIndexError> VectorIndexStore::search(
    const VectorIndexKey & query,
    VectorSearchRequest request
) const
{
    return backend_->search(query, request);
}

std::size_t VectorIndexStore::size() const noexcept
{
    return backend_->size();
}

VectorIndex & VectorIndexStore::backend() noexcept
{
    return *backend_;
}

const VectorIndex & VectorIndexStore::backend() const noexcept
{
    return *backend_;
}

} // namespace litedb::core::vindex
