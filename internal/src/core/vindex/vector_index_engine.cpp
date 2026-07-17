#include "core/vindex/vector_index_engine.hpp"

#include <algorithm>
#include <string>
#include <utility>

#include "core/filesystem/filesystem.hpp"
#include "core/meta/meta_engine.hpp"
#include "core/schema/schema_loader.hpp"
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

VectorIndexEngine::VectorIndexEngine(
    std::filesystem::path data_directory,
    filesystem::FileSystem & filesystem
) noexcept
    : data_directory_(std::move(data_directory))
    , filesystem_(&filesystem)
{
}

std::expected<void, VectorIndexError> VectorIndexEngine::create_index(
    const meta::entry::VectorIndexEntry & index_entry,
    const schema::CollectionSchema & collection_schema,
    const storage::StorageEngine & storage
)
{
    auto descriptor = make_descriptor(index_entry, collection_schema);
    if (!descriptor) {
        return std::unexpected(std::move(descriptor.error()));
    }
    if (indexes_by_id_.contains(descriptor->index_id)) {
        return std::unexpected(make_error(VectorIndexErrorCode::IndexAlreadyExists, "Vector index already exists"));
    }
    auto store = create_store(*descriptor, storage);
    if (!store) {
        return std::unexpected(std::move(store.error()));
    }
    publish(std::move(*store));
    return {};
}

std::expected<void, VectorIndexError> VectorIndexEngine::restore_all(
    const meta::MetaEngine & catalog,
    const storage::StorageEngine & storage
)
{
    VectorIndexEngine restored {data_directory_, *filesystem_};
    for (const auto * database : catalog.list_databases()) {
        if (database == nullptr) {
            continue;
        }
        for (const auto * collection : catalog.list_collections(database->id())) {
            if (collection == nullptr) {
                continue;
            }
            if (!storage.contains_collection(collection->id())) {
                return std::unexpected(make_error(
                    VectorIndexErrorCode::InvalidMetadata,
                    "Vector index collection is absent from storage"
                ));
            }
            auto collection_schema = schema::load_collection_schema(catalog, collection->id());
            if (!collection_schema) {
                return std::unexpected(make_error(
                    VectorIndexErrorCode::InvalidMetadata,
                    std::move(collection_schema.error().message)
                ));
            }
            for (const auto * entry : catalog.list_vector_indexes(collection->id())) {
                if (entry == nullptr) {
                    continue;
                }
                auto descriptor = make_descriptor(*entry, *collection_schema);
                if (!descriptor) {
                    return std::unexpected(std::move(descriptor.error()));
                }
                auto store = restored.restore_store(*descriptor, storage);
                if (!store && (store.error().code == VectorIndexErrorCode::IndexFileMissing ||
                               store.error().code == VectorIndexErrorCode::CorruptedIndex ||
                               store.error().code == VectorIndexErrorCode::StaleIndex)) {
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
    return {};
}

std::expected<void, VectorIndexError> VectorIndexEngine::drop_index(common::VIndexId index_id)
{
    auto found = indexes_by_id_.find(index_id);
    if (found == indexes_by_id_.end()) {
        return std::unexpected(make_error(VectorIndexErrorCode::IndexNotFound, "Vector index not found"));
    }

    const auto descriptor = found->second.descriptor();
    auto collection = indexes_by_collection_.find(descriptor.collection_id);
    if (collection != indexes_by_collection_.end()) {
        auto & ids = collection->second;
        ids.erase(std::remove(ids.begin(), ids.end(), index_id), ids.end());
        if (ids.empty()) {
            indexes_by_collection_.erase(collection);
        }
    }

    indexes_by_id_.erase(found);
    if (descriptor.kind == VectorIndexKind::Hnsw) {
        auto removed = filesystem_->remove(index_path(index_id));
        if (!removed) {
            return std::unexpected(make_error(VectorIndexErrorCode::FileSystemFailure, std::move(removed.error().message)));
        }
    }
    return {};
}

std::expected<void, VectorIndexError> VectorIndexEngine::drop_collection_indexes(common::CollectionId collection_id)
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

std::expected<void, VectorIndexError> VectorIndexEngine::insert(
    common::VIndexId index_id,
    const VectorIndexKey & key,
    common::RecordId record_id
)
{
    auto * index = find_store(index_id);
    if (index == nullptr) {
        return std::unexpected(make_error(VectorIndexErrorCode::IndexNotFound, "Vector index not found"));
    }
    return index->insert(key, record_id);
}

std::expected<void, VectorIndexError> VectorIndexEngine::erase(common::VIndexId index_id, common::RecordId record_id)
{
    auto * index = find_store(index_id);
    if (index == nullptr) {
        return std::unexpected(make_error(VectorIndexErrorCode::IndexNotFound, "Vector index not found"));
    }
    return index->erase(record_id);
}

std::expected<VectorIndexKeyBindings, VectorIndexError> VectorIndexEngine::prepare_insert(
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
        const auto * managed = find_store(index_id);
        if (managed == nullptr) {
            return std::unexpected(make_error(VectorIndexErrorCode::IndexNotFound, "Vector index not found"));
        }
        auto key = key_from_record(record_data, managed->descriptor().column_ordinal);
        if (!key) {
            return std::unexpected(std::move(key.error()));
        }
        if (*key) {
            bindings.push_back(VectorIndexKeyBinding {.index_id = index_id, .key = std::move(**key)});
        }
    }
    return bindings;
}

std::expected<void, VectorIndexError> VectorIndexEngine::on_insert(
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

std::expected<VectorIndexUpdateBindings, VectorIndexError> VectorIndexEngine::prepare_update(
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
        const auto * managed = find_store(index_id);
        if (managed == nullptr) {
            return std::unexpected(make_error(VectorIndexErrorCode::IndexNotFound, "Vector index not found"));
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

std::expected<void, VectorIndexError> VectorIndexEngine::on_update(
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

std::expected<VectorIndexKeyBindings, VectorIndexError> VectorIndexEngine::prepare_delete(
    common::CollectionId collection_id,
    const schema::RecordData & old_record_data
) const
{
    return prepare_insert(collection_id, old_record_data);
}

std::expected<void, VectorIndexError> VectorIndexEngine::on_delete(
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

std::expected<std::vector<VectorSearchResult>, VectorIndexError> VectorIndexEngine::search(
    common::VIndexId index_id,
    const VectorIndexKey & query,
    VectorSearchRequest request
) const
{
    const auto * index = find_store(index_id);
    if (index == nullptr) {
        return std::unexpected(make_error(VectorIndexErrorCode::IndexNotFound, "Vector index not found"));
    }
    return index->search(query, request);
}

std::optional<ManagedVectorIndexView> VectorIndexEngine::find_index(common::VIndexId index_id) const noexcept
{
    const auto * index = find_store(index_id);
    if (index == nullptr) {
        return std::nullopt;
    }
    return make_view(*index);
}

std::vector<ManagedVectorIndexView> VectorIndexEngine::list_indexes(common::CollectionId collection_id) const
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

void VectorIndexEngine::clear() noexcept
{
    indexes_by_id_.clear();
    indexes_by_collection_.clear();
}

std::expected<VectorIndexDescriptor, VectorIndexError> VectorIndexEngine::make_descriptor(
    const meta::entry::VectorIndexEntry & index_entry,
    const schema::CollectionSchema & collection_schema
)
{
    if (index_entry.collection_id() != collection_schema.collection_id()) {
        return std::unexpected(make_error(VectorIndexErrorCode::InvalidMetadata, "Vector index collection metadata is invalid"));
    }
    const auto * column = collection_schema.find_column(index_entry.column_id());
    if (column == nullptr || column->type().id != common::LogicalTypeId::Vector ||
        !column->type().parameter || *column->type().parameter != index_entry.dimension()) {
        return std::unexpected(make_error(VectorIndexErrorCode::InvalidMetadata, "Vector index column metadata is invalid"));
    }
    if (index_entry.dimension() == 0) {
        return std::unexpected(make_error(VectorIndexErrorCode::InvalidMetadata, "Vector index dimension must be greater than zero"));
    }

    VectorDistanceMetric metric;
    switch (index_entry.metric()) {
    case meta::entry::VectorDistanceMetric::L2: metric = VectorDistanceMetric::L2; break;
    case meta::entry::VectorDistanceMetric::InnerProduct: metric = VectorDistanceMetric::InnerProduct; break;
    case meta::entry::VectorDistanceMetric::Cosine: metric = VectorDistanceMetric::Cosine; break;
    default:
        return std::unexpected(make_error(VectorIndexErrorCode::InvalidMetadata, "Unsupported vector distance metric metadata"));
    }

    VectorIndexKind kind;
    switch (index_entry.index_kind()) {
    case meta::entry::VectorIndexKind::Hnsw: kind = VectorIndexKind::Hnsw; break;
    default:
        return std::unexpected(make_error(VectorIndexErrorCode::InvalidMetadata, "Unsupported vector index kind metadata"));
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
    if (descriptor.kind == VectorIndexKind::Hnsw) {
        auto exists = filesystem_->exists(index_path(descriptor.index_id));
        if (!exists) {
            return std::unexpected(make_error(VectorIndexErrorCode::FileSystemFailure, std::move(exists.error().message)));
        }
        if (*exists) {
            auto removed = filesystem_->remove(index_path(descriptor.index_id));
            if (!removed) {
                return std::unexpected(make_error(VectorIndexErrorCode::FileSystemFailure, std::move(removed.error().message)));
            }
        }
    }
    auto backend = make_backend(descriptor, storage, false);
    if (!backend) {
        return std::unexpected(std::move(backend.error()));
    }
    if (descriptor.kind == VectorIndexKind::Hnsw) {
        auto built = build_from_storage(**backend, descriptor, storage);
        if (!built) {
            backend->reset();
            (void) filesystem_->remove(index_path(descriptor.index_id));
            return std::unexpected(std::move(built.error()));
        }
    }
    return VectorIndexStore {descriptor, std::move(*backend)};
}

std::expected<VectorIndexStore, VectorIndexError> VectorIndexEngine::restore_store(
    const VectorIndexDescriptor & descriptor,
    const storage::StorageEngine & storage
) const
{
    auto exists = filesystem_->exists(index_path(descriptor.index_id));
    if (!exists) {
        return std::unexpected(make_error(VectorIndexErrorCode::FileSystemFailure, std::move(exists.error().message)));
    }
    if (!*exists) {
        return std::unexpected(make_error(VectorIndexErrorCode::IndexFileMissing, "Persisted vector index file is missing"));
    }
    auto backend = make_backend(descriptor, storage, true);
    if (!backend) {
        return std::unexpected(std::move(backend.error()));
    }
    const auto * hnsw = dynamic_cast<const HnswIndex *>(backend->get());
    if (hnsw == nullptr) {
        return std::unexpected(make_error(VectorIndexErrorCode::CorruptedIndex, "HNSW backend type mismatch"));
    }
    auto verified = verify_against_storage(*hnsw, descriptor, storage);
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
    bool restore
) const
{
    switch (descriptor.kind) {
    case VectorIndexKind::Flat:
        return std::unique_ptr<VectorIndex> {std::make_unique<FlatIndex>(FlatIndexOptions {
            .collection_id = descriptor.collection_id,
            .column_ordinal = descriptor.column_ordinal,
            .dimension = descriptor.dimension,
            .metric = descriptor.metric,
        }, storage)};
    case VectorIndexKind::Hnsw: {
        HnswIndexOptions options {
            .dimension = descriptor.dimension,
            .metric = descriptor.metric,
            .max_neighbors = descriptor.max_neighbors,
            .ef_construction = descriptor.ef_construction,
            .ef_search_default = descriptor.ef_search_default,
            .random_seed = descriptor.random_seed,
        };
        auto index = restore
            ? HnswIndex::open(index_path(descriptor.index_id), descriptor.index_id, descriptor.collection_id,
                              descriptor.column_id, options, *filesystem_)
            : HnswIndex::create(index_path(descriptor.index_id), descriptor.index_id, descriptor.collection_id,
                                descriptor.column_id, options, *filesystem_);
        if (!index) {
            return std::unexpected(std::move(index.error()));
        }
        return std::unique_ptr<VectorIndex> {std::make_unique<HnswIndex>(std::move(*index))};
    }
    }
    return std::unexpected(make_error(VectorIndexErrorCode::UnsupportedIndexKind, "Unsupported vector index kind"));
}

std::expected<void, VectorIndexError> VectorIndexEngine::build_from_storage(
    VectorIndex & index,
    const VectorIndexDescriptor & descriptor,
    const storage::StorageEngine & storage
)
{
    auto cursor = storage.scan(descriptor.collection_id);
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
        if (descriptor.column_ordinal >= record.data.values.size()) {
            return std::unexpected(make_error(VectorIndexErrorCode::StorageFailure, "Vector column ordinal is outside the stored record"));
        }
        const auto & value = record.data.values[descriptor.column_ordinal];
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

std::expected<void, VectorIndexError> VectorIndexEngine::verify_against_storage(
    const HnswIndex & index,
    const VectorIndexDescriptor & descriptor,
    const storage::StorageEngine & storage
)
{
    auto cursor = storage.scan(descriptor.collection_id);
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
        auto key = key_from_record(record.data, descriptor.column_ordinal);
        if (!key) {
            return std::unexpected(std::move(key.error()));
        }
        if (!*key) {
            continue;
        }
        ++expected_size;
        if (!index.matches_record(record.record_id, **key)) {
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

} // namespace litedb::core::vindex
