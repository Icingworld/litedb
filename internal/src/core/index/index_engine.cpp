#include "core/index/index_engine.hpp"

#include <algorithm>
#include <utility>

#include "core/meta/meta_engine.hpp"
#include "core/index/btree_index/btree_index.hpp"
#include "core/index/hash_index/hash_index.hpp"
#include "core/schema/schema_loader.hpp"
#include "core/storage/storage_engine.hpp"

namespace litedb::core::index
{

namespace
{

/**
 * @brief 创建索引错误
 */
IndexError make_error(IndexErrorCode code, std::string message)
{
    return IndexError {code, std::move(message)};
}

} // namespace

std::unique_ptr<ScalarIndex> IndexEngine::make_backend(meta::entry::IndexKind index_kind)
{
    switch (index_kind) {
    case meta::entry::IndexKind::Hash:
        return std::make_unique<HashIndex>();
    case meta::entry::IndexKind::BTree:
        return std::make_unique<BTreeIndex>();
    }
    return nullptr;
}

std::expected<std::optional<ScalarIndexKey>, IndexError> IndexEngine::make_key_from_record(
    const schema::RecordData & record_data,
    std::size_t column_ordinal,
    const common::LogicalType & key_type
)
{
    if (column_ordinal >= record_data.values.size()) {
        return std::unexpected(make_error(IndexErrorCode::InvalidIndexColumn, "Indexed column ordinal is out of range"));
    }

    const auto & value = record_data.values[column_ordinal];
    if (value.is_null()) {
        return std::nullopt;
    }
    if (!value.matches_type(key_type)) {
        return std::unexpected(make_error(
            IndexErrorCode::KeyTypeMismatch,
            "Index key type does not match indexed column type"
        ));
    }

    auto key = ScalarIndexKey::from_value(value);
    if (!key.has_value()) {
        return std::unexpected(std::move(key.error()));
    }
    return std::optional<ScalarIndexKey>(std::move(key.value()));
}

ManagedIndexView IndexEngine::make_view(const IndexStore & store) noexcept
{
    const auto & descriptor = store.descriptor();
    return ManagedIndexView {
        .index_id = descriptor.index_id,
        .collection_id = descriptor.collection_id,
        .column_id = descriptor.column_id,
        .column_ordinal = descriptor.column_ordinal,
        .key_type = descriptor.key_type,
        .kind = descriptor.kind,
        .unique = descriptor.unique,
        .entry_count = store.size(),
    };
}

const IndexStore * IndexEngine::find_store(common::IndexId index_id) const noexcept
{
    const auto it = stores_by_id_.find(index_id);
    if (it == stores_by_id_.end()) {
        return nullptr;
    }
    return &it->second;
}

IndexStore * IndexEngine::find_store(common::IndexId index_id) noexcept
{
    const auto it = stores_by_id_.find(index_id);
    if (it == stores_by_id_.end()) {
        return nullptr;
    }
    return &it->second;
}

std::vector<const IndexStore *> IndexEngine::list_stores(
    common::CollectionId collection_id
) const
{
    std::vector<const IndexStore *> indexes;
    const auto it = indexes_by_collection_.find(collection_id);
    if (it == indexes_by_collection_.end()) {
        return indexes;
    }

    indexes.reserve(it->second.size());
    for (const auto index_id : it->second) {
        if (const auto * store = find_store(index_id); store != nullptr) {
            indexes.push_back(store);
        }
    }
    return indexes;
}

std::expected<void, IndexError> IndexEngine::build_index_from_storage(
    IndexStore & store,
    const storage::StorageEngine & storage
) const
{
    const auto & descriptor = store.descriptor();
    auto cursor = storage.scan(descriptor.collection_id);
    if (!cursor) return std::unexpected(make_error(IndexErrorCode::StorageError, cursor.error().message));
    while (true) {
        auto next = cursor->next();
        if (!next) return std::unexpected(make_error(IndexErrorCode::StorageError, next.error().message));
        if (!*next) break;
        const auto & record = **next;
        auto key = make_key_from_record(record.data, descriptor.column_ordinal, descriptor.key_type);
        if (!key.has_value()) {
            return std::unexpected(std::move(key.error()));
        }
        if (!key->has_value()) {
            continue;
        }

        auto inserted = store.insert(key->value(), record.record_id);
        if (!inserted.has_value()) {
            return std::unexpected(std::move(inserted.error()));
        }
    }
    return {};
}

std::expected<void, IndexError> IndexEngine::create_index(
    const meta::entry::IndexEntry & index_entry,
    const schema::CollectionSchema & collection_schema,
    const storage::StorageEngine & storage
)
{
    if (find_store(index_entry.id()) != nullptr) {
        return std::unexpected(make_error(IndexErrorCode::IndexAlreadyExists, "Index already exists"));
    }

    const auto column_id = index_entry.column_id();
    if (!column_id.has_value()) {
        return std::unexpected(make_error(IndexErrorCode::InvalidIndexColumn, "Index has no columns"));
    }
    const auto * column = collection_schema.find_column(column_id.value());
    if (column == nullptr) {
        return std::unexpected(make_error(IndexErrorCode::InvalidIndexColumn, "Indexed column is not in collection schema"));
    }
    if (column->type().id == common::LogicalTypeId::Vector) {
        return std::unexpected(make_error(IndexErrorCode::InvalidIndexColumn, "VECTOR column cannot use scalar index"));
    }

    auto index = make_backend(index_entry.kind());
    if (index == nullptr) {
        return std::unexpected(make_error(IndexErrorCode::InvalidIndexColumn, "Unsupported index kind"));
    }

    IndexStore store {IndexDescriptor {
        .index_id = index_entry.id(),
        .collection_id = index_entry.collection_id(),
        .column_id = column_id.value(),
        .column_ordinal = column->ordinal(),
        .key_type = column->type(),
        .kind = index->kind(),
        .unique = index_entry.unique(),
    }, std::move(index)};

    auto built = build_index_from_storage(store, storage);
    if (!built.has_value()) {
        return std::unexpected(std::move(built.error()));
    }

    const auto index_id = store.descriptor().index_id;
    const auto collection_id = store.descriptor().collection_id;
    stores_by_id_.emplace(index_id, std::move(store));
    indexes_by_collection_[collection_id].push_back(index_id);
    return {};
}

std::expected<void, IndexError> IndexEngine::drop_index(common::IndexId index_id)
{
    const auto it = stores_by_id_.find(index_id);
    if (it == stores_by_id_.end()) {
        return std::unexpected(make_error(IndexErrorCode::IndexNotFound, "Index not found"));
    }

    const auto collection_id = it->second.descriptor().collection_id;
    stores_by_id_.erase(it);

    const auto collection_it = indexes_by_collection_.find(collection_id);
    if (collection_it != indexes_by_collection_.end()) {
        auto & index_ids = collection_it->second;
        std::erase(index_ids, index_id);
        if (index_ids.empty()) {
            indexes_by_collection_.erase(collection_it);
        }
    }
    return {};
}

void IndexEngine::drop_collection_indexes(common::CollectionId collection_id)
{
    const auto it = indexes_by_collection_.find(collection_id);
    if (it == indexes_by_collection_.end()) {
        return;
    }

    for (const auto index_id : it->second) {
        stores_by_id_.erase(index_id);
    }
    indexes_by_collection_.erase(it);
}

std::expected<void, IndexError> IndexEngine::rebuild_all(
    const meta::MetaEngine & catalog,
    const storage::StorageEngine & storage
)
{
    IndexEngine rebuilt;

    for (const auto * database : catalog.list_databases()) {
        if (database == nullptr) {
            continue;
        }

        for (const auto * collection : catalog.list_collections(database->id())) {
            if (collection == nullptr) {
                continue;
            }

            if (!storage.contains_collection(collection->id())) {
                continue;
            }

            auto collection_schema = schema::load_collection_schema(catalog, collection->id());
            if (!collection_schema.has_value()) {
                return std::unexpected(make_error(
                    IndexErrorCode::InvalidIndexColumn,
                    collection_schema.error().message
                ));
            }

            for (const auto * index_entry : catalog.list_indexes(collection->id())) {
                if (index_entry == nullptr) {
                    continue;
                }

                auto created = rebuilt.create_index(*index_entry, collection_schema.value(), storage);
                if (!created.has_value()) {
                    return std::unexpected(std::move(created.error()));
                }
            }
        }
    }

    *this = std::move(rebuilt);
    return {};
}

std::expected<IndexKeyBindings, IndexError> IndexEngine::prepare_insert(
    common::CollectionId collection_id,
    const schema::RecordData & record_data
) const
{
    IndexKeyBindings bindings;
    for (const auto * store : list_stores(collection_id)) {
        auto key = make_key_from_record(record_data, store->descriptor().column_ordinal, store->descriptor().key_type);
        if (!key.has_value()) {
            return std::unexpected(std::move(key.error()));
        }
        if (!key->has_value()) {
            continue;
        }

        auto unique = store->validate_insert(key->value());
        if (!unique.has_value()) {
            return std::unexpected(std::move(unique.error()));
        }

        bindings.push_back(IndexKeyBinding {
            .index_id = store->descriptor().index_id,
            .key = std::move(key->value()),
        });
    }
    return bindings;
}

std::expected<void, IndexError> IndexEngine::on_insert(
    common::RecordId record_id,
    const IndexKeyBindings & bindings
)
{
    IndexKeyBindings inserted_bindings;
    inserted_bindings.reserve(bindings.size());

    for (const auto & binding : bindings) {
        auto * store = find_store(binding.index_id);
        if (store == nullptr) {
            return std::unexpected(make_error(IndexErrorCode::IndexNotFound, "Index not found"));
        }
        auto inserted = store->insert(binding.key, record_id);
        if (!inserted.has_value()) {
            for (auto it = inserted_bindings.rbegin(); it != inserted_bindings.rend(); ++it) {
                if (auto * rollback_index = find_store(it->index_id); rollback_index != nullptr) {
                    (void) rollback_index->erase(it->key, record_id);
                }
            }
            return std::unexpected(std::move(inserted.error()));
        }
        inserted_bindings.push_back(binding);
    }
    return {};
}

std::expected<IndexUpdateBindings, IndexError> IndexEngine::prepare_update(
    common::CollectionId collection_id,
    const schema::RecordData & old_record_data,
    const schema::RecordData & new_record_data
) const
{
    IndexUpdateBindings bindings;
    for (const auto * store : list_stores(collection_id)) {
        auto old_key = make_key_from_record(old_record_data, store->descriptor().column_ordinal, store->descriptor().key_type);
        if (!old_key.has_value()) {
            return std::unexpected(std::move(old_key.error()));
        }

        auto new_key = make_key_from_record(new_record_data, store->descriptor().column_ordinal, store->descriptor().key_type);
        if (!new_key.has_value()) {
            return std::unexpected(std::move(new_key.error()));
        }

        IndexUpdateBinding binding {
            .index_id = store->descriptor().index_id,
            .old_key = std::move(old_key.value()),
            .new_key = std::move(new_key.value()),
        };

        if (!binding.old_key.has_value() && !binding.new_key.has_value()) {
            binding.key_changed = false;
        } else if (!binding.old_key.has_value() || !binding.new_key.has_value()) {
            binding.key_changed = true;
        } else {
            binding.key_changed = !ScalarIndexEqual {}(binding.old_key.value(), binding.new_key.value());
        }

        if (binding.key_changed && binding.new_key.has_value()) {
            auto unique = store->validate_insert(binding.new_key.value());
            if (!unique.has_value()) {
                return std::unexpected(std::move(unique.error()));
            }
        }

        bindings.push_back(std::move(binding));
    }
    return bindings;
}

std::expected<void, IndexError> IndexEngine::on_update(
    common::RecordId record_id,
    const IndexUpdateBindings & bindings
)
{
    std::vector<IndexUpdateBinding> applied_bindings;

    for (const auto & binding : bindings) {
        auto * store = find_store(binding.index_id);
        if (store == nullptr) {
            return std::unexpected(make_error(IndexErrorCode::IndexNotFound, "Index not found"));
        }

        if (!binding.key_changed) {
            continue;
        }

        if (binding.new_key.has_value()) {
            auto inserted = store->insert(binding.new_key.value(), record_id);
            if (!inserted.has_value()) {
                for (auto it = applied_bindings.rbegin(); it != applied_bindings.rend(); ++it) {
                    if (auto * rollback_index = find_store(it->index_id); rollback_index != nullptr) {
                        if (it->new_key.has_value()) {
                            (void) rollback_index->erase(it->new_key.value(), record_id);
                        }
                        if (it->old_key.has_value()) {
                            (void) rollback_index->insert(it->old_key.value(), record_id);
                        }
                    }
                }
                return std::unexpected(std::move(inserted.error()));
            }
        }

        if (binding.old_key.has_value()) {
            auto erased = store->erase(binding.old_key.value(), record_id);
            if (!erased.has_value()) {
                if (binding.new_key.has_value()) {
                    (void) store->erase(binding.new_key.value(), record_id);
                }
                for (auto it = applied_bindings.rbegin(); it != applied_bindings.rend(); ++it) {
                    if (auto * rollback_index = find_store(it->index_id); rollback_index != nullptr) {
                        if (it->new_key.has_value()) {
                            (void) rollback_index->erase(it->new_key.value(), record_id);
                        }
                        if (it->old_key.has_value()) {
                            (void) rollback_index->insert(it->old_key.value(), record_id);
                        }
                    }
                }
                return std::unexpected(std::move(erased.error()));
            }
        }

        applied_bindings.push_back(binding);
    }
    return {};
}

std::expected<IndexKeyBindings, IndexError> IndexEngine::prepare_delete(
    common::CollectionId collection_id,
    const schema::RecordData & old_record_data
) const
{
    IndexKeyBindings bindings;
    for (const auto * store : list_stores(collection_id)) {
        auto key = make_key_from_record(old_record_data, store->descriptor().column_ordinal, store->descriptor().key_type);
        if (!key.has_value()) {
            return std::unexpected(std::move(key.error()));
        }
        if (!key->has_value()) {
            continue;
        }

        bindings.push_back(IndexKeyBinding {
            .index_id = store->descriptor().index_id,
            .key = std::move(key->value()),
        });
    }
    return bindings;
}

std::expected<void, IndexError> IndexEngine::on_delete(
    common::RecordId record_id,
    const IndexKeyBindings & bindings
)
{
    IndexKeyBindings erased_bindings;
    erased_bindings.reserve(bindings.size());

    for (const auto & binding : bindings) {
        auto * store = find_store(binding.index_id);
        if (store == nullptr) {
            return std::unexpected(make_error(IndexErrorCode::IndexNotFound, "Index not found"));
        }
        auto erased = store->erase(binding.key, record_id);
        if (!erased.has_value()) {
            for (auto it = erased_bindings.rbegin(); it != erased_bindings.rend(); ++it) {
                if (auto * rollback_index = find_store(it->index_id); rollback_index != nullptr) {
                    (void) rollback_index->insert(it->key, record_id);
                }
            }
            return std::unexpected(std::move(erased.error()));
        }
        erased_bindings.push_back(binding);
    }
    return {};
}

std::optional<ManagedIndexView> IndexEngine::find_index(common::IndexId index_id) const noexcept
{
    const auto * store = find_store(index_id);
    if (store == nullptr) {
        return std::nullopt;
    }
    return make_view(*store);
}

std::vector<ManagedIndexView> IndexEngine::list_indexes(common::CollectionId collection_id) const
{
    std::vector<ManagedIndexView> views;
    for (const auto * store : list_stores(collection_id)) {
        views.push_back(make_view(*store));
    }
    return views;
}

std::vector<ManagedIndexView> IndexEngine::find_indexes_for_column(
    common::CollectionId collection_id,
    common::ColumnId column_id
) const
{
    std::vector<ManagedIndexView> views;
    for (const auto * store : list_stores(collection_id)) {
        if (store->descriptor().column_id == column_id) {
            views.push_back(make_view(*store));
        }
    }
    return views;
}

std::expected<std::vector<common::RecordId>, IndexError> IndexEngine::find_equal(
    common::IndexId index_id,
    const ScalarIndexKey & key
) const
{
    const auto * store = find_store(index_id);
    if (store == nullptr) {
        return std::unexpected(make_error(IndexErrorCode::IndexNotFound, "Index not found"));
    }
    return store->find_equal(key);
}

std::expected<std::vector<common::RecordId>, IndexError> IndexEngine::scan_range(
    common::IndexId index_id,
    const IndexRange & range
) const
{
    const auto * store = find_store(index_id);
    if (store == nullptr) {
        return std::unexpected(make_error(IndexErrorCode::IndexNotFound, "Index not found"));
    }
    return store->scan_range(range);
}

void IndexEngine::clear() noexcept
{
    stores_by_id_.clear();
    indexes_by_collection_.clear();
}

} // namespace litedb::core::index
