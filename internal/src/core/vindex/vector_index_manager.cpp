#include "core/vindex/vector_index_manager.hpp"

#include <algorithm>
#include <string>
#include <utility>

#include "core/vindex/flat_index/flat_index.hpp"

namespace litedb::core::vindex
{

namespace
{

[[nodiscard]]
VectorIndexError make_error(VectorIndexErrorCode code, std::string message)
{
    return VectorIndexError {code, std::move(message)};
}

} // namespace

VectorIndexManager::VectorIndexManager(const storage::StorageEngine & storage) noexcept
    : storage_(&storage)
{
}

std::expected<void, VectorIndexError> VectorIndexManager::create_index(const VectorIndexDefinition & definition)
{
    if (indexes_by_id_.contains(definition.index_id)) {
        return std::unexpected(make_error(VectorIndexErrorCode::IndexAlreadyExists, "Vector index already exists"));
    }
    if (definition.kind != VectorIndexKind::Flat) {
        return std::unexpected(make_error(VectorIndexErrorCode::UnsupportedIndexKind, "Unsupported vector index kind"));
    }
    if (definition.dimension == 0) {
        return std::unexpected(make_error(VectorIndexErrorCode::InvalidDimension, "Vector index dimension must be greater than 0"));
    }

    auto index = make_index(definition);
    if (!index) {
        return std::unexpected(make_error(VectorIndexErrorCode::UnsupportedIndexKind, "Unsupported vector index kind"));
    }

    indexes_by_id_.emplace(definition.index_id, ManagedVectorIndex {
        .index_id = definition.index_id,
        .collection_id = definition.collection_id,
        .column_id = definition.column_id,
        .kind = definition.kind,
        .index = std::move(index),
    });
    indexes_by_collection_[definition.collection_id].push_back(definition.index_id);
    return {};
}

std::expected<void, VectorIndexError> VectorIndexManager::drop_index(common::VIndexId index_id)
{
    auto found = indexes_by_id_.find(index_id);
    if (found == indexes_by_id_.end()) {
        return std::unexpected(make_error(VectorIndexErrorCode::IndexNotFound, "Vector index not found"));
    }

    auto collection = indexes_by_collection_.find(found->second.collection_id);
    if (collection != indexes_by_collection_.end()) {
        auto & ids = collection->second;
        ids.erase(std::remove(ids.begin(), ids.end(), index_id), ids.end());
        if (ids.empty()) {
            indexes_by_collection_.erase(collection);
        }
    }

    indexes_by_id_.erase(found);
    return {};
}

void VectorIndexManager::drop_collection_indexes(common::CollectionId collection_id)
{
    auto collection = indexes_by_collection_.find(collection_id);
    if (collection == indexes_by_collection_.end()) {
        return;
    }

    for (const auto index_id : collection->second) {
        indexes_by_id_.erase(index_id);
    }
    indexes_by_collection_.erase(collection);
}

std::expected<void, VectorIndexError> VectorIndexManager::insert(
    common::VIndexId index_id,
    const VectorIndexKey & key,
    common::RecordId record_id
)
{
    auto * index = find_managed_index(index_id);
    if (index == nullptr) {
        return std::unexpected(make_error(VectorIndexErrorCode::IndexNotFound, "Vector index not found"));
    }
    return index->index->insert(key, record_id);
}

std::expected<void, VectorIndexError> VectorIndexManager::erase(common::VIndexId index_id, common::RecordId record_id)
{
    auto * index = find_managed_index(index_id);
    if (index == nullptr) {
        return std::unexpected(make_error(VectorIndexErrorCode::IndexNotFound, "Vector index not found"));
    }
    return index->index->erase(record_id);
}

std::expected<std::vector<VectorSearchResult>, VectorIndexError> VectorIndexManager::search(
    common::VIndexId index_id,
    const VectorIndexKey & query,
    VectorSearchRequest request
) const
{
    const auto * index = find_managed_index(index_id);
    if (index == nullptr) {
        return std::unexpected(make_error(VectorIndexErrorCode::IndexNotFound, "Vector index not found"));
    }
    return index->index->search(query, request);
}

std::optional<ManagedVectorIndexView> VectorIndexManager::find_index(common::VIndexId index_id) const noexcept
{
    const auto * index = find_managed_index(index_id);
    if (index == nullptr) {
        return std::nullopt;
    }
    return make_view(*index);
}

std::vector<ManagedVectorIndexView> VectorIndexManager::list_indexes(common::CollectionId collection_id) const
{
    std::vector<ManagedVectorIndexView> views;
    const auto collection = indexes_by_collection_.find(collection_id);
    if (collection == indexes_by_collection_.end()) {
        return views;
    }

    views.reserve(collection->second.size());
    for (const auto index_id : collection->second) {
        const auto * index = find_managed_index(index_id);
        if (index != nullptr) {
            views.push_back(make_view(*index));
        }
    }
    return views;
}

void VectorIndexManager::clear() noexcept
{
    indexes_by_id_.clear();
    indexes_by_collection_.clear();
}

std::unique_ptr<VectorIndex> VectorIndexManager::make_index(const VectorIndexDefinition & definition) const
{
    switch (definition.kind) {
    case VectorIndexKind::Flat:
        return std::make_unique<FlatIndex>(FlatIndexOptions {
            .collection_id = definition.collection_id,
            .column_ordinal = definition.column_ordinal,
            .dimension = definition.dimension,
            .metric = definition.metric,
        }, *storage_);
    case VectorIndexKind::Hnsw:
        return nullptr;
    }

    return nullptr;
}

ManagedVectorIndexView VectorIndexManager::make_view(const ManagedVectorIndex & managed_index) const noexcept
{
    return ManagedVectorIndexView {
        .index_id = managed_index.index_id,
        .collection_id = managed_index.collection_id,
        .column_id = managed_index.column_id,
        .kind = managed_index.kind,
        .index = *managed_index.index,
    };
}

VectorIndexManager::ManagedVectorIndex * VectorIndexManager::find_managed_index(common::VIndexId index_id) noexcept
{
    auto found = indexes_by_id_.find(index_id);
    if (found == indexes_by_id_.end()) {
        return nullptr;
    }
    return &found->second;
}

const VectorIndexManager::ManagedVectorIndex * VectorIndexManager::find_managed_index(common::VIndexId index_id) const noexcept
{
    auto found = indexes_by_id_.find(index_id);
    if (found == indexes_by_id_.end()) {
        return nullptr;
    }
    return &found->second;
}

} // namespace litedb::core::vindex
