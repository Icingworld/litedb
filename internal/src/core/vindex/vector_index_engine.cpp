#include "core/vindex/vector_index_engine.hpp"

#include <algorithm>
#include <chrono>
#include <limits>
#include <string>
#include <utility>

#include "core/filesystem/filesystem.hpp"
#include "core/catalog/catalog_viewer.hpp"
#include "core/storage/schema_loader.hpp"
#include "core/storage/storage_engine.hpp"
#include "core/vindex/flat_index/flat_index.hpp"
#include "core/vindex/hnsw_index/hnsw_index.hpp"

namespace litedb::core::vindex
{

namespace
{

[[nodiscard]]
VectorIndexError make_error(VectorIndexErrorCode code, std::string message)
{
    return VectorIndexError {code, message};
}

[[nodiscard]]
VectorIndexError source_error(
    VectorIndexErrorCode code,
    error::Error source,
    VectorIndexOperation operation,
    common::VIndexId index_id = 0,
    common::CollectionId collection_id = 0,
    const std::filesystem::path & path = {}
)
{
    return VectorIndexError {
        code,
        source.message(),
        VectorIndexErrorContext {
            .operation = operation,
            .index_id = index_id,
            .collection_id = collection_id,
            .path = path,
            .source_code = source.encode_code(),
        },
    };
}

[[nodiscard]]
bool is_rebuildable_error(const VectorIndexError & error) noexcept
{
    return error.is(VectorIndexErrorCode::IndexFileMissing) ||
           error.is(VectorIndexErrorCode::CorruptedIndex) ||
           error.is(VectorIndexErrorCode::UnsupportedVersion) ||
           error.is(VectorIndexErrorCode::ChecksumMismatch) ||
           error.is(VectorIndexErrorCode::CorruptedGraph) ||
           error.is(VectorIndexErrorCode::StaleIndex);
}

[[nodiscard]]
std::expected<std::optional<VectorIndexKey>, VectorIndexError>
key_from_record(const common::RecordData & record_data, std::size_t column_ordinal)
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

VectorIndexEngine::VectorIndexEngine(
    std::filesystem::path data_directory,
    filesystem::FileSystem & filesystem
) noexcept
    : data_directory_(std::move(data_directory))
    , filesystem_(&filesystem)
{}

std::expected<void, VectorIndexError> VectorIndexEngine::create_index(
    const catalog::entry::VectorIndexEntry & index_entry,
    const schema::CollectionSchema & collection_schema,
    const storage::StorageEngine & storage
)
{
    auto descriptor = make_descriptor(index_entry, collection_schema);
    if (!descriptor) {
        return std::unexpected(std::move(descriptor.error()));
    }
    if (indexes_by_id_.contains(descriptor->index_id)) {
        return std::unexpected(
            make_error(VectorIndexErrorCode::IndexAlreadyExists, "Vector index already exists")
        );
    }
    auto store = create_store(*descriptor, storage);
    if (!store) {
        return std::unexpected(std::move(store.error()));
    }
    publish(std::move(*store));
    return {};
}

std::expected<void, VectorIndexError> VectorIndexEngine::restore_all(
    const catalog::CatalogViewer & catalog,
    const storage::StorageEngine & storage
)
{
    auto cleaned = cleanup_stale_temporary_files();
    if (!cleaned) {
        return std::unexpected(std::move(cleaned.error()));
    }
    VectorIndexEngine restored {data_directory_, *filesystem_};
    for (const auto & database_reference : catalog.list_databases()) {
        const auto & database = database_reference.get();
        for (const auto & collection_reference : catalog.list_collections(database.id())) {
            const auto & collection = collection_reference.get();
            if (!storage.contains_collection(collection.id())) {
                return std::unexpected(make_error(
                    VectorIndexErrorCode::InvalidMetadata,
                    "Vector index collection is absent from storage"
                ));
            }
            auto collection_schema = storage::load_collection_schema(catalog, collection.id());
            if (!collection_schema) {
                return std::unexpected(make_error(
                    VectorIndexErrorCode::InvalidMetadata,
                    collection_schema.error().message()
                ));
            }
            for (const auto & entry_reference : catalog.list_vector_indexes(collection.id())) {
                const auto & entry = entry_reference.get();
                auto descriptor = make_descriptor(entry, *collection_schema);
                if (!descriptor) {
                    return std::unexpected(std::move(descriptor.error()));
                }
                auto store = restored.restore_store(*descriptor, storage);
                if (!store && is_rebuildable_error(store.error())) {
                    store = restored.rebuild_store(*descriptor, storage);
                }
                if (!store) {
                    return std::unexpected(std::move(store.error()));
                }
                restored.publish(std::move(*store));
            }
        }
    }
    indexes_by_id_ = std::move(restored.indexes_by_id_);
    indexes_by_collection_ = std::move(restored.indexes_by_collection_);
    dirty_collections_.clear();
    last_compaction_reclaimed_bytes_ = 0;
    last_compaction_duration_us_ = 0;
    return {};
}

std::expected<void, VectorIndexError> VectorIndexEngine::reload_collection(
    const catalog::CatalogViewer & catalog,
    const storage::StorageEngine & storage,
    common::CollectionId collection_id
)
{
    if (!storage.contains_collection(collection_id)) {
        return std::unexpected(make_error(
            VectorIndexErrorCode::InvalidMetadata,
            "Vector index collection is absent from storage"
        ));
    }
    auto collection_schema = storage::load_collection_schema(catalog, collection_id);
    if (!collection_schema) {
        return std::unexpected(
            make_error(VectorIndexErrorCode::InvalidMetadata, collection_schema.error().message())
        );
    }

    VectorIndexEngine restored {data_directory_, *filesystem_};
    for (const auto & entry_reference : catalog.list_vector_indexes(collection_id)) {
        const auto & entry = entry_reference.get();
        auto descriptor = make_descriptor(entry, *collection_schema);
        if (!descriptor)
            return std::unexpected(std::move(descriptor.error()));

        auto load_store = [&]() -> std::expected<VectorIndexStore, VectorIndexError> {
            if (descriptor->kind == VectorIndexKind::Hnsw) {
                return restored.restore_store(*descriptor, storage);
            }
            auto backend = restored.make_backend(*descriptor, storage, true);
            if (!backend)
                return std::unexpected(std::move(backend.error()));
            return VectorIndexStore {*descriptor, std::move(*backend)};
        };
        auto store = load_store();
        if (!store && descriptor->kind == VectorIndexKind::Hnsw &&
            is_rebuildable_error(store.error())) {
            store = restored.rebuild_store(*descriptor, storage);
        }
        if (!store)
            return std::unexpected(std::move(store.error()));
        restored.publish(std::move(*store));
    }

    if (const auto current = indexes_by_collection_.find(collection_id);
        current != indexes_by_collection_.end()) {
        for (const auto index_id : current->second)
            indexes_by_id_.erase(index_id);
        indexes_by_collection_.erase(current);
    }
    for (auto & [index_id, store] : restored.indexes_by_id_) {
        indexes_by_id_.emplace(index_id, std::move(store));
    }
    if (const auto ids = restored.indexes_by_collection_.find(collection_id);
        ids != restored.indexes_by_collection_.end()) {
        indexes_by_collection_.emplace(collection_id, std::move(ids->second));
    }
    dirty_collections_.erase(collection_id);
    return {};
}

std::expected<void, VectorIndexError> VectorIndexEngine::drop_index(common::VIndexId index_id)
{
    auto found = indexes_by_id_.find(index_id);
    if (found == indexes_by_id_.end()) {
        return std::unexpected(
            make_error(VectorIndexErrorCode::IndexNotFound, "Vector index not found")
        );
    }

    const auto descriptor = found->second.descriptor();
    if (descriptor.kind == VectorIndexKind::Hnsw) {
        auto removed = filesystem_->remove(index_path(index_id));
        if (!removed) {
            return std::unexpected(source_error(
                VectorIndexErrorCode::FileSystemFailure,
                std::move(removed.error()),
                VectorIndexOperation::Drop,
                index_id,
                descriptor.collection_id,
                index_path(index_id)
            ));
        }
    }

    auto collection = indexes_by_collection_.find(descriptor.collection_id);
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

std::expected<void, VectorIndexError> VectorIndexEngine::drop_collection_indexes(
    common::CollectionId collection_id
)
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

std::expected<void, VectorIndexError> VectorIndexEngine::checkpoint(
    const storage::StorageEngine & storage
)
{
    constexpr std::size_t MinimumTombstones = 1024;
    constexpr std::uint64_t MinimumFileBytes = 64ULL << 20U;

    for (auto & [index_id, store] : indexes_by_id_) {
        if (store.descriptor().kind != VectorIndexKind::Hnsw) {
            continue;
        }
        auto & hnsw = static_cast<HnswIndex &>(store.backend());
        const auto before = hnsw.stats();
        const auto tombstone_threshold =
            before.tombstone_count >= MinimumTombstones &&
            before.tombstone_count >= (before.physical_node_count + 3) / 4;
        const auto size_threshold =
            before.file_bytes >= MinimumFileBytes &&
            before.estimated_compact_bytes <= std::numeric_limits<std::uint64_t>::max() / 2 &&
            before.file_bytes >= before.estimated_compact_bytes * 2;
        if (!tombstone_threshold && !size_threshold) {
            continue;
        }

        const auto started = std::chrono::steady_clock::now();
        const auto descriptor = store.descriptor();
        const auto final_path = index_path(index_id);
        auto temporary_path = final_path;
        temporary_path += ".compact";
        auto temporary_exists = filesystem_->exists(temporary_path);
        if (!temporary_exists) {
            return std::unexpected(source_error(
                VectorIndexErrorCode::FileSystemFailure,
                std::move(temporary_exists.error()),
                VectorIndexOperation::Compact,
                index_id,
                descriptor.collection_id,
                temporary_path
            ));
        }
        if (*temporary_exists) {
            auto removed = filesystem_->remove(temporary_path);
            if (!removed) {
                return std::unexpected(source_error(
                    VectorIndexErrorCode::FileSystemFailure,
                    std::move(removed.error()),
                    VectorIndexOperation::Compact,
                    index_id,
                    descriptor.collection_id,
                    temporary_path
                ));
            }
        }

        auto compacted_backend = make_backend(descriptor, storage, false, temporary_path);
        if (!compacted_backend) {
            return std::unexpected(std::move(compacted_backend.error()));
        }
        auto built = build_from_storage(**compacted_backend, descriptor, storage);
        if (!built) {
            compacted_backend->reset();
            (void)filesystem_->remove(temporary_path);
            return std::unexpected(std::move(built.error()));
        }
        compacted_backend->reset();

        auto closed = hnsw.close();
        if (!closed) {
            (void)filesystem_->remove(temporary_path);
            return std::unexpected(std::move(closed.error()));
        }
        auto replaced = filesystem_->replace_file_atomic(temporary_path, final_path);
        if (!replaced) {
            auto reopened = make_backend(descriptor, storage, true, final_path);
            if (reopened) {
                store = VectorIndexStore {descriptor, std::move(*reopened)};
            } else {
                dirty_collections_.insert(descriptor.collection_id);
                return std::unexpected(make_error(
                    VectorIndexErrorCode::RecoveryRequired,
                    "HNSW compaction publication failed and the previous store could not be reopened"
                ));
            }
            (void)filesystem_->remove(temporary_path);
            return std::unexpected(source_error(
                VectorIndexErrorCode::FileSystemFailure,
                std::move(replaced.error()),
                VectorIndexOperation::Publish,
                index_id,
                descriptor.collection_id,
                final_path
            ));
        }

        std::optional<VectorIndexError> directory_error;
        const auto parent = final_path.parent_path();
        if (!parent.empty()) {
            auto synced = filesystem_->sync_directory(parent);
            if (!synced && !synced.error().is(filesystem::FileSystemErrorCode::Unsupported)) {
                directory_error.emplace(source_error(
                    VectorIndexErrorCode::DurabilityUnknown,
                    std::move(synced.error()),
                    VectorIndexOperation::Sync,
                    index_id,
                    descriptor.collection_id,
                    parent
                ));
            }
        }

        auto reopened = make_backend(descriptor, storage, true, final_path);
        if (!reopened) {
            dirty_collections_.insert(descriptor.collection_id);
            return std::unexpected(std::move(reopened.error()));
        }
        const auto & rebuilt_hnsw = static_cast<const HnswIndex &>(**reopened);
        const auto after = rebuilt_hnsw.stats();
        store = VectorIndexStore {descriptor, std::move(*reopened)};
        last_compaction_reclaimed_bytes_ =
            before.file_bytes > after.file_bytes ? before.file_bytes - after.file_bytes : 0;
        last_compaction_duration_us_ =
            static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                                           std::chrono::steady_clock::now() - started
            )
                                           .count());
        if (directory_error) {
            return std::unexpected(std::move(*directory_error));
        }
    }
    return {};
}

std::expected<void, VectorIndexError> VectorIndexEngine::insert(
    common::VIndexId index_id,
    const VectorIndexKey & key,
    common::RecordId record_id
)
{
    auto * index = find_store(index_id);
    if (index == nullptr) {
        return std::unexpected(
            make_error(VectorIndexErrorCode::IndexNotFound, "Vector index not found")
        );
    }
    return index->insert(key, record_id);
}

std::expected<void, VectorIndexError>
VectorIndexEngine::erase(common::VIndexId index_id, common::RecordId record_id)
{
    auto * index = find_store(index_id);
    if (index == nullptr) {
        return std::unexpected(
            make_error(VectorIndexErrorCode::IndexNotFound, "Vector index not found")
        );
    }
    return index->erase(record_id);
}

std::expected<VectorIndexKeyBindings, VectorIndexError> VectorIndexEngine::prepare_insert(
    common::CollectionId collection_id,
    const common::RecordData & record_data
) const
{
    VectorIndexKeyBindings bindings;
    if (dirty_collections_.contains(collection_id)) {
        return std::unexpected(make_error(
            VectorIndexErrorCode::RecoveryRequired,
            "Vector indexes for the collection require reload"
        ));
    }
    const auto found = indexes_by_collection_.find(collection_id);
    if (found == indexes_by_collection_.end()) {
        return bindings;
    }
    bindings.reserve(found->second.size());
    for (const auto index_id : found->second) {
        const auto * managed = find_store(index_id);
        if (managed == nullptr) {
            return std::unexpected(
                make_error(VectorIndexErrorCode::IndexNotFound, "Vector index not found")
            );
        }
        auto key = key_from_record(record_data, managed->descriptor().column_ordinal);
        if (!key) {
            return std::unexpected(std::move(key.error()));
        }
        if (*key) {
            bindings.push_back(
                VectorIndexKeyBinding {.index_id = index_id, .key = std::move(**key)}
            );
        }
    }
    return bindings;
}

std::expected<void, VectorIndexError>
VectorIndexEngine::on_insert(common::RecordId record_id, const VectorIndexKeyBindings & bindings)
{
    VectorIndexKeyBindings applied;
    applied.reserve(bindings.size());
    for (const auto & binding : bindings) {
        auto inserted = insert(binding.index_id, binding.key, record_id);
        if (!inserted) {
            bool rollback_failed = false;
            for (auto it = applied.rbegin(); it != applied.rend(); ++it) {
                auto rolled_back = erase(it->index_id, record_id);
                rollback_failed = rollback_failed || !rolled_back;
            }
            if (rollback_failed) {
                mark_recovery_required(binding.index_id);
                return std::unexpected(make_error(
                    VectorIndexErrorCode::RecoveryRequired,
                    "Vector index insert rollback failed; collection reload is required"
                ));
            }
            return std::unexpected(std::move(inserted.error()));
        }
        applied.push_back(binding);
    }
    return {};
}

std::expected<VectorIndexUpdateBindings, VectorIndexError> VectorIndexEngine::prepare_update(
    common::CollectionId collection_id,
    const common::RecordData & old_record_data,
    const common::RecordData & new_record_data
) const
{
    VectorIndexUpdateBindings bindings;
    if (dirty_collections_.contains(collection_id)) {
        return std::unexpected(make_error(
            VectorIndexErrorCode::RecoveryRequired,
            "Vector indexes for the collection require reload"
        ));
    }
    const auto found = indexes_by_collection_.find(collection_id);
    if (found == indexes_by_collection_.end()) {
        return bindings;
    }
    bindings.reserve(found->second.size());
    for (const auto index_id : found->second) {
        const auto * managed = find_store(index_id);
        if (managed == nullptr) {
            return std::unexpected(
                make_error(VectorIndexErrorCode::IndexNotFound, "Vector index not found")
            );
        }
        auto old_key = key_from_record(old_record_data, managed->descriptor().column_ordinal);
        if (!old_key) {
            return std::unexpected(std::move(old_key.error()));
        }
        auto new_key = key_from_record(new_record_data, managed->descriptor().column_ordinal);
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

std::expected<void, VectorIndexError>
VectorIndexEngine::on_update(common::RecordId record_id, const VectorIndexUpdateBindings & bindings)
{
    std::vector<const VectorIndexUpdateBinding *> applied;
    for (const auto & binding : bindings) {
        if (!binding.key_changed) {
            continue;
        }
        if (binding.old_key) {
            auto erased = erase(binding.index_id, record_id);
            if (!erased) {
                bool rollback_failed = false;
                for (auto it = applied.rbegin(); it != applied.rend(); ++it) {
                    if ((*it)->new_key) {
                        auto removed = erase((*it)->index_id, record_id);
                        rollback_failed = rollback_failed || !removed;
                    }
                    if ((*it)->old_key) {
                        auto restored = insert((*it)->index_id, *(*it)->old_key, record_id);
                        rollback_failed = rollback_failed || !restored;
                    }
                }
                if (rollback_failed) {
                    mark_recovery_required(binding.index_id);
                    return std::unexpected(make_error(
                        VectorIndexErrorCode::RecoveryRequired,
                        "Vector index update rollback failed; collection reload is required"
                    ));
                }
                return std::unexpected(std::move(erased.error()));
            }
        }
        if (binding.new_key) {
            auto inserted = insert(binding.index_id, *binding.new_key, record_id);
            if (!inserted) {
                bool rollback_failed = false;
                if (binding.old_key) {
                    auto restored = insert(binding.index_id, *binding.old_key, record_id);
                    rollback_failed = rollback_failed || !restored;
                }
                for (auto it = applied.rbegin(); it != applied.rend(); ++it) {
                    if ((*it)->new_key) {
                        auto removed = erase((*it)->index_id, record_id);
                        rollback_failed = rollback_failed || !removed;
                    }
                    if ((*it)->old_key) {
                        auto restored = insert((*it)->index_id, *(*it)->old_key, record_id);
                        rollback_failed = rollback_failed || !restored;
                    }
                }
                if (rollback_failed) {
                    mark_recovery_required(binding.index_id);
                    return std::unexpected(make_error(
                        VectorIndexErrorCode::RecoveryRequired,
                        "Vector index update rollback failed; collection reload is required"
                    ));
                }
                return std::unexpected(std::move(inserted.error()));
            }
        }
        applied.push_back(&binding);
    }
    return {};
}

std::expected<VectorIndexKeyBindings, VectorIndexError> VectorIndexEngine::prepare_delete(
    common::CollectionId collection_id,
    const common::RecordData & old_record_data
) const
{
    return prepare_insert(collection_id, old_record_data);
}

std::expected<void, VectorIndexError>
VectorIndexEngine::on_delete(common::RecordId record_id, const VectorIndexKeyBindings & bindings)
{
    VectorIndexKeyBindings applied;
    applied.reserve(bindings.size());
    for (const auto & binding : bindings) {
        auto erased = erase(binding.index_id, record_id);
        if (!erased) {
            bool rollback_failed = false;
            for (auto it = applied.rbegin(); it != applied.rend(); ++it) {
                auto restored = insert(it->index_id, it->key, record_id);
                rollback_failed = rollback_failed || !restored;
            }
            if (rollback_failed) {
                mark_recovery_required(binding.index_id);
                return std::unexpected(make_error(
                    VectorIndexErrorCode::RecoveryRequired,
                    "Vector index delete rollback failed; collection reload is required"
                ));
            }
            return std::unexpected(std::move(erased.error()));
        }
        applied.push_back(binding);
    }
    return {};
}

std::expected<std::vector<VectorSearchResult>, VectorIndexError> VectorIndexEngine::search(
    common::VIndexId index_id,
    const VectorIndexKey & query,
    VectorSearchRequest request
) const
{
    const auto * index = find_store(index_id);
    if (index == nullptr) {
        return std::unexpected(
            make_error(VectorIndexErrorCode::IndexNotFound, "Vector index not found")
        );
    }
    if (dirty_collections_.contains(index->descriptor().collection_id)) {
        return std::unexpected(make_error(
            VectorIndexErrorCode::RecoveryRequired,
            "Vector indexes for the collection require reload"
        ));
    }
    return index->search(query, request);
}

std::optional<ManagedVectorIndexView> VectorIndexEngine::find_index(
    common::VIndexId index_id
) const noexcept
{
    const auto * index = find_store(index_id);
    if (index == nullptr) {
        return std::nullopt;
    }
    return make_view(*index);
}

std::vector<ManagedVectorIndexView> VectorIndexEngine::list_indexes(
    common::CollectionId collection_id
) const
{
    std::vector<ManagedVectorIndexView> views;
    const auto collection = indexes_by_collection_.find(collection_id);
    if (collection == indexes_by_collection_.end()) {
        return views;
    }

    views.reserve(collection->second.size());
    for (const auto index_id : collection->second) {
        const auto * index = find_store(index_id);
        if (index != nullptr) {
            views.push_back(make_view(*index));
        }
    }
    return views;
}

VectorIndexMaintenanceStats VectorIndexEngine::maintenance_stats() const noexcept
{
    VectorIndexMaintenanceStats result {
        .last_compaction_reclaimed_bytes = last_compaction_reclaimed_bytes_,
        .last_compaction_duration_us = last_compaction_duration_us_,
    };
    for (const auto & [index_id, store] : indexes_by_id_) {
        if (store.descriptor().kind != VectorIndexKind::Hnsw) {
            continue;
        }
        const auto & hnsw = static_cast<const HnswIndex &>(store.backend());
        const auto stats = hnsw.stats();
        result.frame_count += stats.frame_count;
        result.physical_node_count += stats.physical_node_count;
        result.active_count += stats.active_count;
        result.tombstone_count += stats.tombstone_count;
        result.file_bytes += stats.file_bytes;
    }
    return result;
}

void VectorIndexEngine::clear() noexcept
{
    indexes_by_id_.clear();
    indexes_by_collection_.clear();
    dirty_collections_.clear();
    last_compaction_reclaimed_bytes_ = 0;
    last_compaction_duration_us_ = 0;
}

std::expected<VectorIndexDescriptor, VectorIndexError> VectorIndexEngine::make_descriptor(
    const catalog::entry::VectorIndexEntry & index_entry,
    const schema::CollectionSchema & collection_schema
)
{
    if (index_entry.collection_id() != collection_schema.collection_id()) {
        return std::unexpected(make_error(
            VectorIndexErrorCode::InvalidMetadata,
            "Vector index collection metadata is invalid"
        ));
    }
    const auto column = collection_schema.find_column(index_entry.column_id());
    if (!column.has_value() || column->type().id != common::LogicalTypeId::Vector ||
        !column->type().parameter || *column->type().parameter != index_entry.dimension()) {
        return std::unexpected(make_error(
            VectorIndexErrorCode::InvalidMetadata,
            "Vector index column metadata is invalid"
        ));
    }
    if (index_entry.dimension() == 0) {
        return std::unexpected(make_error(
            VectorIndexErrorCode::InvalidMetadata,
            "Vector index dimension must be greater than zero"
        ));
    }
    if (index_entry.dimension() > (1U << 20U) || index_entry.max_neighbors() == 0 ||
        index_entry.max_neighbors() > (1U << 20U) ||
        index_entry.ef_construction() < index_entry.max_neighbors() ||
        index_entry.ef_construction() > (1U << 24U) || index_entry.ef_search_default() == 0 ||
        index_entry.ef_search_default() > (1U << 24U)) {
        return std::unexpected(make_error(
            VectorIndexErrorCode::InvalidMetadata,
            "HNSW metadata parameters are outside the supported range"
        ));
    }

    VectorDistanceMetric metric;
    switch (index_entry.metric()) {
    case catalog::entry::VectorDistanceMetric::L2:
        metric = VectorDistanceMetric::L2;
        break;
    case catalog::entry::VectorDistanceMetric::InnerProduct:
        metric = VectorDistanceMetric::InnerProduct;
        break;
    case catalog::entry::VectorDistanceMetric::Cosine:
        metric = VectorDistanceMetric::Cosine;
        break;
    default:
        return std::unexpected(make_error(
            VectorIndexErrorCode::InvalidMetadata,
            "Unsupported vector distance metric metadata"
        ));
    }

    VectorIndexKind kind;
    switch (index_entry.index_kind()) {
    case catalog::entry::VectorIndexKind::Hnsw:
        kind = VectorIndexKind::Hnsw;
        break;
    default:
        return std::unexpected(make_error(
            VectorIndexErrorCode::InvalidMetadata,
            "Unsupported vector index kind metadata"
        ));
    }

    return VectorIndexDescriptor {
        .index_id = index_entry.id(),
        .collection_id = index_entry.collection_id(),
        .column_id = index_entry.column_id(),
        .column_ordinal = column->ordinal(),
        .dimension = index_entry.dimension(),
        .kind = kind,
        .metric = metric,
        .max_neighbors = index_entry.max_neighbors(),
        .ef_construction = index_entry.ef_construction(),
        .ef_search_default = index_entry.ef_search_default(),
        .random_seed = index_entry.random_seed(),
    };
}

std::expected<VectorIndexStore, VectorIndexError> VectorIndexEngine::create_store(
    const VectorIndexDescriptor & descriptor,
    const storage::StorageEngine & storage
) const
{
    if (descriptor.kind != VectorIndexKind::Hnsw) {
        auto backend = make_backend(descriptor, storage, false);
        if (!backend) {
            return std::unexpected(std::move(backend.error()));
        }
        return VectorIndexStore {descriptor, std::move(*backend)};
    }

    const auto final_path = index_path(descriptor.index_id);
    auto temporary_path = final_path;
    temporary_path += ".building";
    auto temporary_exists = filesystem_->exists(temporary_path);
    if (!temporary_exists) {
        return std::unexpected(source_error(
            VectorIndexErrorCode::FileSystemFailure,
            std::move(temporary_exists.error()),
            VectorIndexOperation::Create,
            descriptor.index_id,
            descriptor.collection_id,
            temporary_path
        ));
    }
    if (*temporary_exists) {
        auto removed = filesystem_->remove(temporary_path);
        if (!removed) {
            return std::unexpected(source_error(
                VectorIndexErrorCode::FileSystemFailure,
                std::move(removed.error()),
                VectorIndexOperation::Drop,
                descriptor.index_id,
                descriptor.collection_id,
                temporary_path
            ));
        }
    }

    auto backend = make_backend(descriptor, storage, false, temporary_path);
    if (!backend) {
        return std::unexpected(std::move(backend.error()));
    }
    auto built = build_from_storage(**backend, descriptor, storage);
    if (!built) {
        backend->reset();
        (void)filesystem_->remove(temporary_path);
        return std::unexpected(std::move(built.error()));
    }
    backend->reset();

    auto replaced = filesystem_->replace_file_atomic(temporary_path, final_path);
    if (!replaced) {
        (void)filesystem_->remove(temporary_path);
        return std::unexpected(source_error(
            VectorIndexErrorCode::FileSystemFailure,
            std::move(replaced.error()),
            VectorIndexOperation::Publish,
            descriptor.index_id,
            descriptor.collection_id,
            final_path
        ));
    }
    const auto parent = final_path.parent_path();
    if (!parent.empty()) {
        auto synced = filesystem_->sync_directory(parent);
        if (!synced && !synced.error().is(filesystem::FileSystemErrorCode::Unsupported)) {
            return std::unexpected(source_error(
                VectorIndexErrorCode::DurabilityUnknown,
                std::move(synced.error()),
                VectorIndexOperation::Sync,
                descriptor.index_id,
                descriptor.collection_id,
                parent
            ));
        }
    }

    auto opened = make_backend(descriptor, storage, true, final_path);
    if (!opened) {
        return std::unexpected(std::move(opened.error()));
    }
    return VectorIndexStore {descriptor, std::move(*opened)};
}

std::expected<VectorIndexStore, VectorIndexError> VectorIndexEngine::restore_store(
    const VectorIndexDescriptor & descriptor,
    const storage::StorageEngine & storage
) const
{
    auto exists = filesystem_->exists(index_path(descriptor.index_id));
    if (!exists) {
        return std::unexpected(source_error(
            VectorIndexErrorCode::FileSystemFailure,
            std::move(exists.error()),
            VectorIndexOperation::Open,
            descriptor.index_id,
            descriptor.collection_id,
            index_path(descriptor.index_id)
        ));
    }
    if (!*exists) {
        return std::unexpected(make_error(
            VectorIndexErrorCode::IndexFileMissing,
            "Persisted vector index file is missing"
        ));
    }
    auto backend = make_backend(descriptor, storage, true);
    if (!backend) {
        return std::unexpected(std::move(backend.error()));
    }
    if ((*backend)->kind() != VectorIndexKind::Hnsw) {
        return std::unexpected(
            make_error(VectorIndexErrorCode::CorruptedIndex, "HNSW backend type mismatch")
        );
    }
    const auto & hnsw = static_cast<const HnswIndex &>(**backend);
    auto verified = verify_against_storage(hnsw, descriptor, storage);
    if (!verified) {
        return std::unexpected(std::move(verified.error()));
    }
    return VectorIndexStore {descriptor, std::move(*backend)};
}

std::expected<VectorIndexStore, VectorIndexError> VectorIndexEngine::rebuild_store(
    const VectorIndexDescriptor & descriptor,
    const storage::StorageEngine & storage
) const
{
    return create_store(descriptor, storage);
}

std::expected<std::unique_ptr<VectorIndex>, VectorIndexError> VectorIndexEngine::make_backend(
    const VectorIndexDescriptor & descriptor,
    const storage::StorageEngine & storage,
    bool restore,
    std::filesystem::path path
) const
{
    switch (descriptor.kind) {
    case VectorIndexKind::Flat:
        return std::unique_ptr<VectorIndex> {std::make_unique<FlatIndex>(
            FlatIndexOptions {
                .collection_id = descriptor.collection_id,
                .column_ordinal = descriptor.column_ordinal,
                .dimension = descriptor.dimension,
                .metric = descriptor.metric,
            },
            storage
        )};
    case VectorIndexKind::Hnsw: {
        if (path.empty()) {
            path = index_path(descriptor.index_id);
        }
        HnswIndexOptions options {
            .dimension = descriptor.dimension,
            .metric = descriptor.metric,
            .max_neighbors = descriptor.max_neighbors,
            .ef_construction = descriptor.ef_construction,
            .ef_search_default = descriptor.ef_search_default,
            .random_seed = descriptor.random_seed,
        };
        auto index = restore ? HnswIndex::open(
                                   path,
                                   descriptor.index_id,
                                   descriptor.collection_id,
                                   descriptor.column_id,
                                   options,
                                   *filesystem_
                               )
                             : HnswIndex::create(
                                   path,
                                   descriptor.index_id,
                                   descriptor.collection_id,
                                   descriptor.column_id,
                                   options,
                                   *filesystem_
                               );
        if (!index) {
            return std::unexpected(std::move(index.error()));
        }
        return std::unique_ptr<VectorIndex> {std::make_unique<HnswIndex>(std::move(*index))};
    }
    }
    return std::unexpected(
        make_error(VectorIndexErrorCode::UnsupportedIndexKind, "Unsupported vector index kind")
    );
}

std::expected<void, VectorIndexError> VectorIndexEngine::build_from_storage(
    VectorIndex & index,
    const VectorIndexDescriptor & descriptor,
    const storage::StorageEngine & storage
)
{
    auto cursor = storage.scan(descriptor.collection_id);
    if (!cursor) {
        return std::unexpected(source_error(
            VectorIndexErrorCode::StorageFailure,
            std::move(cursor.error()),
            VectorIndexOperation::Build,
            descriptor.index_id,
            descriptor.collection_id
        ));
    }
    while (true) {
        auto next = cursor->next();
        if (!next) {
            return std::unexpected(source_error(
                VectorIndexErrorCode::StorageFailure,
                std::move(next.error()),
                VectorIndexOperation::Build,
                descriptor.index_id,
                descriptor.collection_id
            ));
        }
        if (!*next) {
            break;
        }
        const auto & record = **next;
        if (descriptor.column_ordinal >= record.data.values.size()) {
            return std::unexpected(make_error(
                VectorIndexErrorCode::StorageFailure,
                "Vector column ordinal is outside the stored record"
            ));
        }
        const auto & value = record.data.values[descriptor.column_ordinal];
        if (value.is_null()) {
            continue;
        }
        auto key = VectorIndexKey::from_value(value);
        if (!key) {
            return std::unexpected(std::move(key.error()));
        }
        auto inserted = index.insert(*key, record.id);
        if (!inserted) {
            return inserted;
        }
    }
    return {};
}

std::expected<void, VectorIndexError> VectorIndexEngine::verify_against_storage(
    const HnswIndex & index,
    const VectorIndexDescriptor & descriptor,
    const storage::StorageEngine & storage
)
{
    auto cursor = storage.scan(descriptor.collection_id);
    if (!cursor) {
        return std::unexpected(source_error(
            VectorIndexErrorCode::StorageFailure,
            std::move(cursor.error()),
            VectorIndexOperation::Verify,
            descriptor.index_id,
            descriptor.collection_id
        ));
    }
    std::size_t expected_size = 0;
    while (true) {
        auto next = cursor->next();
        if (!next) {
            return std::unexpected(source_error(
                VectorIndexErrorCode::StorageFailure,
                std::move(next.error()),
                VectorIndexOperation::Verify,
                descriptor.index_id,
                descriptor.collection_id
            ));
        }
        if (!*next) {
            break;
        }
        const auto & record = **next;
        auto key = key_from_record(record.data, descriptor.column_ordinal);
        if (!key) {
            return std::unexpected(std::move(key.error()));
        }
        if (!*key) {
            continue;
        }
        ++expected_size;
        if (!index.matches_record(record.id, **key)) {
            return std::unexpected(make_error(
                VectorIndexErrorCode::StaleIndex,
                "Persisted HNSW index does not match collection storage"
            ));
        }
    }
    if (index.size() != expected_size) {
        return std::unexpected(make_error(
            VectorIndexErrorCode::StaleIndex,
            "Persisted HNSW index contains records absent from collection storage"
        ));
    }
    return {};
}

std::filesystem::path VectorIndexEngine::index_path(common::VIndexId index_id) const
{
    return data_directory_ / ("vindex_" + std::to_string(index_id) + ".lhnsw");
}

std::expected<void, VectorIndexError> VectorIndexEngine::cleanup_stale_temporary_files() const
{
    auto exists = filesystem_->exists(data_directory_);
    if (!exists) {
        return std::unexpected(source_error(
            VectorIndexErrorCode::FileSystemFailure,
            std::move(exists.error()),
            VectorIndexOperation::Restore,
            0,
            0,
            data_directory_
        ));
    }
    if (!*exists) {
        return {};
    }
    auto entries = filesystem_->list_dir(data_directory_);
    if (!entries) {
        return std::unexpected(source_error(
            VectorIndexErrorCode::FileSystemFailure,
            std::move(entries.error()),
            VectorIndexOperation::Restore,
            0,
            0,
            data_directory_
        ));
    }
    for (const auto & entry : *entries) {
        const auto name = entry.filename().string();
        if (!name.ends_with(".building") && !name.ends_with(".compact")) {
            continue;
        }
        const auto path = data_directory_ / entry;
        auto removed = filesystem_->remove(path);
        if (!removed) {
            return std::unexpected(source_error(
                VectorIndexErrorCode::FileSystemFailure,
                std::move(removed.error()),
                VectorIndexOperation::Drop,
                0,
                0,
                path
            ));
        }
    }
    return {};
}

ManagedVectorIndexView VectorIndexEngine::make_view(const VectorIndexStore & store) noexcept
{
    const auto & descriptor = store.descriptor();
    return ManagedVectorIndexView {
        .index_id = descriptor.index_id,
        .collection_id = descriptor.collection_id,
        .column_id = descriptor.column_id,
        .column_ordinal = descriptor.column_ordinal,
        .kind = descriptor.kind,
        .metric = descriptor.metric,
        .dimension = descriptor.dimension,
        .entry_count = store.size(),
    };
}

VectorIndexStore * VectorIndexEngine::find_store(common::VIndexId index_id) noexcept
{
    auto found = indexes_by_id_.find(index_id);
    if (found == indexes_by_id_.end()) {
        return nullptr;
    }
    return &found->second;
}

const VectorIndexStore * VectorIndexEngine::find_store(common::VIndexId index_id) const noexcept
{
    auto found = indexes_by_id_.find(index_id);
    if (found == indexes_by_id_.end()) {
        return nullptr;
    }
    return &found->second;
}

void VectorIndexEngine::publish(VectorIndexStore store)
{
    const auto descriptor = store.descriptor();
    indexes_by_id_.emplace(descriptor.index_id, std::move(store));
    indexes_by_collection_[descriptor.collection_id].push_back(descriptor.index_id);
}

void VectorIndexEngine::mark_recovery_required(common::VIndexId index_id) noexcept
{
    const auto * store = find_store(index_id);
    if (store != nullptr) {
        dirty_collections_.insert(store->descriptor().collection_id);
    }
}

} // namespace litedb::core::vindex
