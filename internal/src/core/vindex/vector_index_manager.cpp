#include "core/vindex/vector_index_manager.hpp"

#include <algorithm>
#include <string>
#include <utility>

#include "core/filesystem/filesystem.hpp"
#include "core/vindex/flat_index/flat_index.hpp"
#include "core/vindex/hnsw_index/hnsw_index.hpp"
#include "core/storage/storage_engine.hpp"

namespace litedb::core::vindex
{

namespace
{

[[nodiscard]]
VectorIndexError make_error(VectorIndexErrorCode code, std::string message)
{
    return VectorIndexError {code, std::move(message)};
}

[[nodiscard]]
std::expected<std::optional<VectorIndexKey>, VectorIndexError> key_from_record(
    const schema::RecordData & record_data,
    std::size_t column_ordinal
)
{
    if (column_ordinal >= record_data.values.size()) {
        return std::unexpected(make_error(
            VectorIndexErrorCode::StorageFailure,
            "Vector column ordinal is outside the stored record"
        ));
    }
    const auto & value = record_data.values[column_ordinal];
    if (value.is_null()) {
        return std::optional<VectorIndexKey> {};
    }
    auto key = VectorIndexKey::from_value(value);
    if (!key) {
        return std::unexpected(std::move(key.error()));
    }
    return std::optional<VectorIndexKey> {std::move(*key)};
}

} // namespace

VectorIndexManager::VectorIndexManager(const storage::StorageEngine & storage) noexcept
    : storage_(&storage)
{
}

VectorIndexManager::VectorIndexManager(
    std::filesystem::path data_directory,
    filesystem::FileSystem & filesystem,
    const storage::StorageEngine & storage
) noexcept
    : data_directory_(std::move(data_directory))
    , filesystem_(&filesystem)
    , storage_(&storage)
{
}

std::expected<void, VectorIndexError> VectorIndexManager::create_index(const VectorIndexDefinition & definition)
{
    if (indexes_by_id_.contains(definition.index_id)) {
        return std::unexpected(make_error(VectorIndexErrorCode::IndexAlreadyExists, "Vector index already exists"));
    }
    if (definition.dimension == 0) {
        return std::unexpected(make_error(VectorIndexErrorCode::InvalidDimension, "Vector index dimension must be greater than 0"));
    }

    if (definition.kind == VectorIndexKind::Hnsw && filesystem_ != nullptr) {
        auto exists = filesystem_->exists(index_path(definition.index_id));
        if (!exists) {
            return std::unexpected(make_error(VectorIndexErrorCode::StorageFailure, std::move(exists.error().message)));
        }
        if (*exists) {
            auto removed = filesystem_->remove(index_path(definition.index_id));
            if (!removed) {
                return std::unexpected(make_error(VectorIndexErrorCode::StorageFailure, std::move(removed.error().message)));
            }
        }
    }

    auto index = make_index(definition, false);
    if (!index) {
        return std::unexpected(std::move(index.error()));
    }
    if (definition.kind == VectorIndexKind::Hnsw) {
        auto built = build_from_storage(**index, definition);
        if (!built) {
            index->reset();
            if (filesystem_ != nullptr) {
                (void) filesystem_->remove(index_path(definition.index_id));
            }
            return std::unexpected(std::move(built.error()));
        }
    }

    indexes_by_id_.emplace(definition.index_id, ManagedVectorIndex {
        .index_id = definition.index_id,
        .collection_id = definition.collection_id,
        .column_id = definition.column_id,
        .column_ordinal = definition.column_ordinal,
        .kind = definition.kind,
        .index = std::move(*index),
    });
    indexes_by_collection_[definition.collection_id].push_back(definition.index_id);
    return {};
}

std::expected<void, VectorIndexError> VectorIndexManager::restore_index(const VectorIndexDefinition & definition)
{
    if (indexes_by_id_.contains(definition.index_id)) {
        return std::unexpected(make_error(VectorIndexErrorCode::IndexAlreadyExists, "Vector index already exists"));
    }
    if (definition.dimension == 0) {
        return std::unexpected(make_error(VectorIndexErrorCode::InvalidDimension, "Vector index dimension must be greater than 0"));
    }
    auto index = make_index(definition, true);
    if (!index) {
        return std::unexpected(std::move(index.error()));
    }
    if (definition.kind == VectorIndexKind::Hnsw) {
        const auto * hnsw = dynamic_cast<const HnswIndex *>(index->get());
        if (hnsw == nullptr) {
            return std::unexpected(make_error(VectorIndexErrorCode::StorageFailure, "HNSW backend type mismatch"));
        }
        auto verified = verify_against_storage(*hnsw, definition);
        if (!verified) {
            return verified;
        }
    }
    indexes_by_id_.emplace(definition.index_id, ManagedVectorIndex {
        .index_id = definition.index_id,
        .collection_id = definition.collection_id,
        .column_id = definition.column_id,
        .column_ordinal = definition.column_ordinal,
        .kind = definition.kind,
        .index = std::move(*index),
    });
    indexes_by_collection_[definition.collection_id].push_back(definition.index_id);
    return {};
}

std::expected<void, VectorIndexError> VectorIndexManager::rebuild_index(const VectorIndexDefinition & definition)
{
    if (indexes_by_id_.contains(definition.index_id)) {
        return std::unexpected(make_error(VectorIndexErrorCode::IndexAlreadyExists, "Vector index already exists"));
    }
    if (definition.kind == VectorIndexKind::Hnsw) {
        if (filesystem_ == nullptr) {
            return std::unexpected(make_error(VectorIndexErrorCode::StorageFailure, "Persistent filesystem is not configured"));
        }
        auto exists = filesystem_->exists(index_path(definition.index_id));
        if (!exists) {
            return std::unexpected(make_error(VectorIndexErrorCode::StorageFailure, std::move(exists.error().message)));
        }
        if (*exists) {
            auto removed = filesystem_->remove(index_path(definition.index_id));
            if (!removed) {
                return std::unexpected(make_error(VectorIndexErrorCode::StorageFailure, std::move(removed.error().message)));
            }
        }
    }
    return create_index(definition);
}

std::expected<void, VectorIndexError> VectorIndexManager::drop_index(common::VIndexId index_id)
{
    auto found = indexes_by_id_.find(index_id);
    if (found == indexes_by_id_.end()) {
        return std::unexpected(make_error(VectorIndexErrorCode::IndexNotFound, "Vector index not found"));
    }

    const auto kind = found->second.kind;
    auto collection = indexes_by_collection_.find(found->second.collection_id);
    if (collection != indexes_by_collection_.end()) {
        auto & ids = collection->second;
        ids.erase(std::remove(ids.begin(), ids.end(), index_id), ids.end());
        if (ids.empty()) {
            indexes_by_collection_.erase(collection);
        }
    }

    indexes_by_id_.erase(found);
    if (kind == VectorIndexKind::Hnsw && filesystem_ != nullptr) {
        auto removed = filesystem_->remove(index_path(index_id));
        if (!removed) {
            return std::unexpected(make_error(VectorIndexErrorCode::StorageFailure, std::move(removed.error().message)));
        }
    }
    return {};
}

std::expected<void, VectorIndexError> VectorIndexManager::drop_collection_indexes(common::CollectionId collection_id)
{
    auto collection = indexes_by_collection_.find(collection_id);
    if (collection == indexes_by_collection_.end()) {
        return {};
    }

    const auto index_ids = collection->second;
    for (const auto index_id : index_ids) {
        auto dropped = drop_index(index_id);
        if (!dropped) {
            return dropped;
        }
    }
    return {};
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

std::expected<VectorIndexKeyBindings, VectorIndexError> VectorIndexManager::prepare_insert(
    common::CollectionId collection_id,
    const schema::RecordData & record_data
) const
{
    VectorIndexKeyBindings bindings;
    const auto found = indexes_by_collection_.find(collection_id);
    if (found == indexes_by_collection_.end()) {
        return bindings;
    }
    bindings.reserve(found->second.size());
    for (const auto index_id : found->second) {
        const auto * managed = find_managed_index(index_id);
        if (managed == nullptr) {
            return std::unexpected(make_error(VectorIndexErrorCode::IndexNotFound, "Vector index not found"));
        }
        auto key = key_from_record(record_data, managed->column_ordinal);
        if (!key) {
            return std::unexpected(std::move(key.error()));
        }
        if (*key) {
            bindings.push_back(VectorIndexKeyBinding {.index_id = index_id, .key = std::move(**key)});
        }
    }
    return bindings;
}

std::expected<void, VectorIndexError> VectorIndexManager::on_insert(
    common::RecordId record_id,
    const VectorIndexKeyBindings & bindings
)
{
    VectorIndexKeyBindings applied;
    applied.reserve(bindings.size());
    for (const auto & binding : bindings) {
        auto inserted = insert(binding.index_id, binding.key, record_id);
        if (!inserted) {
            for (auto it = applied.rbegin(); it != applied.rend(); ++it) {
                (void) erase(it->index_id, record_id);
            }
            return inserted;
        }
        applied.push_back(binding);
    }
    return {};
}

std::expected<VectorIndexUpdateBindings, VectorIndexError> VectorIndexManager::prepare_update(
    common::CollectionId collection_id,
    const schema::RecordData & old_record_data,
    const schema::RecordData & new_record_data
) const
{
    VectorIndexUpdateBindings bindings;
    const auto found = indexes_by_collection_.find(collection_id);
    if (found == indexes_by_collection_.end()) {
        return bindings;
    }
    bindings.reserve(found->second.size());
    for (const auto index_id : found->second) {
        const auto * managed = find_managed_index(index_id);
        if (managed == nullptr) {
            return std::unexpected(make_error(VectorIndexErrorCode::IndexNotFound, "Vector index not found"));
        }
        auto old_key = key_from_record(old_record_data, managed->column_ordinal);
        if (!old_key) {
            return std::unexpected(std::move(old_key.error()));
        }
        auto new_key = key_from_record(new_record_data, managed->column_ordinal);
        if (!new_key) {
            return std::unexpected(std::move(new_key.error()));
        }
        VectorIndexUpdateBinding binding {
            .index_id = index_id,
            .old_key = std::move(*old_key),
            .new_key = std::move(*new_key),
        };
        if (binding.old_key && binding.new_key) {
            binding.key_changed = binding.old_key->value() != binding.new_key->value();
        } else {
            binding.key_changed = binding.old_key.has_value() != binding.new_key.has_value();
        }
        bindings.push_back(std::move(binding));
    }
    return bindings;
}

std::expected<void, VectorIndexError> VectorIndexManager::on_update(
    common::RecordId record_id,
    const VectorIndexUpdateBindings & bindings
)
{
    std::vector<const VectorIndexUpdateBinding *> applied;
    for (const auto & binding : bindings) {
        if (!binding.key_changed) {
            continue;
        }
        if (binding.old_key) {
            auto erased = erase(binding.index_id, record_id);
            if (!erased) {
                for (auto it = applied.rbegin(); it != applied.rend(); ++it) {
                    (void) erase((*it)->index_id, record_id);
                    if ((*it)->old_key) {
                        (void) insert((*it)->index_id, *(*it)->old_key, record_id);
                    }
                }
                return erased;
            }
        }
        if (binding.new_key) {
            auto inserted = insert(binding.index_id, *binding.new_key, record_id);
            if (!inserted) {
                if (binding.old_key) {
                    (void) insert(binding.index_id, *binding.old_key, record_id);
                }
                for (auto it = applied.rbegin(); it != applied.rend(); ++it) {
                    (void) erase((*it)->index_id, record_id);
                    if ((*it)->old_key) {
                        (void) insert((*it)->index_id, *(*it)->old_key, record_id);
                    }
                }
                return inserted;
            }
        }
        applied.push_back(&binding);
    }
    return {};
}

std::expected<VectorIndexKeyBindings, VectorIndexError> VectorIndexManager::prepare_delete(
    common::CollectionId collection_id,
    const schema::RecordData & old_record_data
) const
{
    return prepare_insert(collection_id, old_record_data);
}

std::expected<void, VectorIndexError> VectorIndexManager::on_delete(
    common::RecordId record_id,
    const VectorIndexKeyBindings & bindings
)
{
    VectorIndexKeyBindings applied;
    applied.reserve(bindings.size());
    for (const auto & binding : bindings) {
        auto erased = erase(binding.index_id, record_id);
        if (!erased) {
            for (auto it = applied.rbegin(); it != applied.rend(); ++it) {
                (void) insert(it->index_id, it->key, record_id);
            }
            return erased;
        }
        applied.push_back(binding);
    }
    return {};
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

std::expected<std::unique_ptr<VectorIndex>, VectorIndexError> VectorIndexManager::make_index(
    const VectorIndexDefinition & definition,
    bool restore
) const
{
    switch (definition.kind) {
    case VectorIndexKind::Flat:
        return std::unique_ptr<VectorIndex> {std::make_unique<FlatIndex>(FlatIndexOptions {
            .collection_id = definition.collection_id,
            .column_ordinal = definition.column_ordinal,
            .dimension = definition.dimension,
            .metric = definition.metric,
        }, *storage_)};
    case VectorIndexKind::Hnsw: {
        if (filesystem_ == nullptr || data_directory_.empty()) {
            return std::unexpected(make_error(
                VectorIndexErrorCode::StorageFailure,
                "Persistent vector index manager requires a data directory and filesystem"
            ));
        }
        HnswIndexOptions options {
            .dimension = definition.dimension,
            .metric = definition.metric,
            .max_neighbors = definition.max_neighbors,
            .ef_construction = definition.ef_construction,
            .ef_search_default = definition.ef_search_default,
            .random_seed = definition.random_seed,
        };
        auto index = restore
            ? HnswIndex::open(
                index_path(definition.index_id), definition.index_id, definition.collection_id,
                definition.column_id, options, *filesystem_
            )
            : HnswIndex::create(
                index_path(definition.index_id), definition.index_id, definition.collection_id,
                definition.column_id, options, *filesystem_
            );
        if (!index) {
            return std::unexpected(std::move(index.error()));
        }
        return std::unique_ptr<VectorIndex> {std::make_unique<HnswIndex>(std::move(*index))};
    }
    }

    return std::unexpected(make_error(VectorIndexErrorCode::UnsupportedIndexKind, "Unsupported vector index kind"));
}

std::expected<void, VectorIndexError> VectorIndexManager::build_from_storage(
    VectorIndex & index,
    const VectorIndexDefinition & definition
) const
{
    auto cursor = storage_->scan(definition.collection_id);
    if (!cursor) {
        return std::unexpected(make_error(VectorIndexErrorCode::StorageFailure, std::move(cursor.error().message)));
    }
    while (true) {
        auto next = cursor->next();
        if (!next) {
            return std::unexpected(make_error(VectorIndexErrorCode::StorageFailure, std::move(next.error().message)));
        }
        if (!*next) {
            break;
        }
        const auto & record = **next;
        if (definition.column_ordinal >= record.data.values.size()) {
            return std::unexpected(make_error(VectorIndexErrorCode::StorageFailure, "Vector column ordinal is outside the stored record"));
        }
        const auto & value = record.data.values[definition.column_ordinal];
        if (value.is_null()) {
            continue;
        }
        auto key = VectorIndexKey::from_value(value);
        if (!key) {
            return std::unexpected(std::move(key.error()));
        }
        auto inserted = index.insert(*key, record.record_id);
        if (!inserted) {
            return inserted;
        }
    }
    return {};
}

std::expected<void, VectorIndexError> VectorIndexManager::verify_against_storage(
    const HnswIndex & index,
    const VectorIndexDefinition & definition
) const
{
    auto cursor = storage_->scan(definition.collection_id);
    if (!cursor) {
        return std::unexpected(make_error(VectorIndexErrorCode::StorageFailure, std::move(cursor.error().message)));
    }
    std::size_t expected_size = 0;
    while (true) {
        auto next = cursor->next();
        if (!next) {
            return std::unexpected(make_error(VectorIndexErrorCode::StorageFailure, std::move(next.error().message)));
        }
        if (!*next) {
            break;
        }
        const auto & record = **next;
        auto key = key_from_record(record.data, definition.column_ordinal);
        if (!key) {
            return std::unexpected(std::move(key.error()));
        }
        if (!*key) {
            continue;
        }
        ++expected_size;
        if (!index.matches_record(record.record_id, **key)) {
            return std::unexpected(make_error(
                VectorIndexErrorCode::StorageFailure,
                "Persisted HNSW index does not match collection storage"
            ));
        }
    }
    if (index.size() != expected_size) {
        return std::unexpected(make_error(
            VectorIndexErrorCode::StorageFailure,
            "Persisted HNSW index contains records absent from collection storage"
        ));
    }
    return {};
}

std::filesystem::path VectorIndexManager::index_path(common::VIndexId index_id) const
{
    return data_directory_ / ("vindex_" + std::to_string(index_id) + ".lhnsw");
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
