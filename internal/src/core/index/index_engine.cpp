#include "core/index/index_engine.hpp"

#include <algorithm>
#include <utility>

#include "core/meta/meta_engine.hpp"
#include "core/index/btree_index/btree_index.hpp"
#include "core/storage/schema_loader.hpp"
#include "core/storage/storage_engine.hpp"

namespace litedb::core::index
{

namespace
{

/**
 * @brief 创建索引错误
 */
IndexError make_error(
    IndexErrorCode code,
    std::string message,
    IndexErrorContext context = {}
)
{
    return IndexError {code, message, std::move(context)};
}

} // namespace

IndexEngine::IndexEngine(
    std::filesystem::path data_directory,
    filesystem::FileSystem & filesystem
) noexcept
    : data_directory_(std::move(data_directory))
    , filesystem_(&filesystem)
{
}

std::filesystem::path IndexEngine::index_path(common::IndexId index_id) const
{
    return data_directory_ / "indexes" / (std::to_string(index_id) + ".bti");
}

std::expected<std::unique_ptr<ScalarIndex>, IndexError> IndexEngine::create_backend(
    const meta::entry::IndexEntry & index_entry,
    const common::LogicalType & key_type
)
{
    if (index_entry.kind() != meta::entry::IndexKind::BTree) {
        return std::unexpected(make_error(IndexErrorCode::InvalidIndexColumn, "Unsupported index kind"));
    }
    auto created = BTreeIndex::create(index_path(index_entry.id()), index_entry.id(), key_type, *filesystem_);
    if (!created.has_value()) {
        return std::unexpected(make_error(
            IndexErrorCode::StorageError,
            created.error().message(),
            {
                .operation = IndexOperation::Create,
                .index_id = index_entry.id(),
                .path = index_path(index_entry.id()),
                .source_code = created.error().encode_code(),
            }
        ));
    }
    return std::make_unique<BTreeIndex>(std::move(*created));
}

std::expected<std::unique_ptr<ScalarIndex>, IndexError> IndexEngine::restore_backend(
    const meta::entry::IndexEntry & index_entry,
    const common::LogicalType & key_type
)
{
    if (index_entry.kind() != meta::entry::IndexKind::BTree) {
        return std::unexpected(make_error(IndexErrorCode::InvalidIndexColumn, "Unsupported index kind"));
    }
    auto opened = BTreeIndex::open(index_path(index_entry.id()), index_entry.id(), key_type, *filesystem_);
    if (!opened.has_value()) {
        return std::unexpected(make_error(
            IndexErrorCode::StorageError,
            opened.error().message(),
            {
                .operation = IndexOperation::Open,
                .index_id = index_entry.id(),
                .path = index_path(index_entry.id()),
                .source_code = opened.error().encode_code(),
            }
        ));
    }
    return std::make_unique<BTreeIndex>(std::move(*opened));
}

std::expected<std::optional<ScalarIndexKey>, IndexError> IndexEngine::make_key_from_record(
    const common::RecordData & record_data,
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
    return std::optional<ScalarIndexKey>(std::move(*key));
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
    std::vector<ScalarIndexEntry> entries;
    auto cursor = storage.scan(descriptor.collection_id);
    if (!cursor) return std::unexpected(make_error(
        IndexErrorCode::StorageError,
        cursor.error().message(),
        {.operation = IndexOperation::Build, .index_id = descriptor.index_id}
    ));
    while (true) {
        auto next = cursor->next();
        if (!next) return std::unexpected(make_error(
            IndexErrorCode::StorageError,
            next.error().message(),
            {.operation = IndexOperation::Build, .index_id = descriptor.index_id}
        ));
        if (!*next) break;
        const auto & record = **next;
        auto key = make_key_from_record(record.data, descriptor.column_ordinal, descriptor.key_type);
        if (!key.has_value()) {
            return std::unexpected(std::move(key.error()));
        }
        if (!key->has_value()) {
            continue;
        }

        entries.push_back(ScalarIndexEntry {
            .key = std::move(key->value()),
            .record_id = record.record_id,
        });
    }
    return store.bulk_load(std::move(entries));
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
    const auto * column = collection_schema.find_column(*column_id);
    if (column == nullptr) {
        return std::unexpected(make_error(IndexErrorCode::InvalidIndexColumn, "Indexed column is not in collection schema"));
    }
    if (column->type().id == common::LogicalTypeId::Vector) {
        return std::unexpected(make_error(IndexErrorCode::InvalidIndexColumn, "VECTOR column cannot use scalar index"));
    }

    auto index = create_backend(index_entry, column->type());
    if (!index.has_value()) {
        return std::unexpected(std::move(index.error()));
    }

    auto store = std::make_unique<IndexStore>(IndexDescriptor {
        .index_id = index_entry.id(),
        .collection_id = index_entry.collection_id(),
        .column_id = *column_id,
        .column_ordinal = column->ordinal(),
        .key_type = column->type(),
        .kind = (*index)->kind(),
        .unique = index_entry.unique(),
    }, std::move(*index));

    auto built = build_index_from_storage(*store, storage);
    if (!built.has_value()) {
        store.reset();
        if (index_entry.kind() == meta::entry::IndexKind::BTree) {
            auto removed = filesystem_->remove(index_path(index_entry.id()));
            if (!removed.has_value()) {
                return std::unexpected(make_error(
                    IndexErrorCode::StorageError,
                    built.error().message() + "; failed to remove partial index: "
                        + removed.error().message(),
                    {
                        .operation = IndexOperation::Drop,
                        .index_id = index_entry.id(),
                        .path = index_path(index_entry.id()),
                        .source_code = removed.error().encode_code(),
                    }
                ));
            }
        }
        return std::unexpected(std::move(built.error()));
    }

    const auto index_id = store->descriptor().index_id;
    const auto collection_id = store->descriptor().collection_id;
    stores_by_id_.emplace(index_id, std::move(*store));
    indexes_by_collection_[collection_id].push_back(index_id);
    return {};
}

std::expected<void, IndexError> IndexEngine::drop_index(common::IndexId index_id)
{
    const auto it = stores_by_id_.find(index_id);
    if (it == stores_by_id_.end()) {
        return std::unexpected(make_error(IndexErrorCode::IndexNotFound, "Index not found"));
    }

    const auto descriptor = it->second.descriptor();
    stores_by_id_.erase(it);

    const auto collection_it = indexes_by_collection_.find(descriptor.collection_id);
    if (collection_it != indexes_by_collection_.end()) {
        auto & index_ids = collection_it->second;
        std::erase(index_ids, index_id);
        if (index_ids.empty()) {
            indexes_by_collection_.erase(collection_it);
        }
    }

    if (descriptor.kind == IndexKind::BTree) {
        auto removed = filesystem_->remove(index_path(index_id));
        if (!removed.has_value()) {
            auto message = removed.error().message();
            auto reopened = BTreeIndex::open(
                index_path(index_id),
                index_id,
                descriptor.key_type,
                *filesystem_
            );
            if (reopened.has_value()) {
                stores_by_id_.emplace(
                    index_id,
                    IndexStore {
                        descriptor,
                        std::make_unique<BTreeIndex>(std::move(*reopened)),
                    }
                );
                indexes_by_collection_[descriptor.collection_id].push_back(index_id);
            } else {
                message += "; failed to restore in-memory index after remove failure: ";
                message += reopened.error().message();
            }
            return std::unexpected(make_error(
                IndexErrorCode::StorageError,
                std::move(message),
                {
                    .operation = IndexOperation::Drop,
                    .index_id = index_id,
                    .path = index_path(index_id),
                    .source_code = removed.error().encode_code(),
                }
            ));
        }
    }
    return {};
}

std::expected<void, IndexError> IndexEngine::drop_collection_indexes(common::CollectionId collection_id)
{
    const auto it = indexes_by_collection_.find(collection_id);
    if (it == indexes_by_collection_.end()) {
        return {};
    }

    const auto index_ids = it->second;
    for (const auto index_id : index_ids) {
        auto dropped = drop_index(index_id);
        if (!dropped.has_value()) {
            return std::unexpected(std::move(dropped.error()));
        }
    }
    return {};
}

std::expected<void, IndexError> IndexEngine::restore_all(
    const meta::CatalogView & catalog,
    const storage::StorageEngine & storage
)
{
    IndexEngine restored {data_directory_, *filesystem_};

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

            auto collection_schema = storage::load_collection_schema(catalog, collection->id());
            if (!collection_schema.has_value()) {
                return std::unexpected(make_error(
                    IndexErrorCode::InvalidIndexColumn,
                    collection_schema.error().message()
                ));
            }

            for (const auto * index_entry : catalog.list_indexes(collection->id())) {
                if (index_entry == nullptr) {
                    continue;
                }

                const auto column_id = index_entry->column_id();
                if (!column_id.has_value()) {
                    return std::unexpected(make_error(IndexErrorCode::InvalidIndexColumn, "Index has no columns"));
                }
                const auto * column = collection_schema->find_column(*column_id);
                if (column == nullptr || column->type().id == common::LogicalTypeId::Vector) {
                    return std::unexpected(make_error(IndexErrorCode::InvalidIndexColumn, "Indexed column is invalid"));
                }

                auto backend = restored.restore_backend(*index_entry, column->type());
                if (!backend.has_value()) {
                    return std::unexpected(std::move(backend.error()));
                }

                IndexStore store {IndexDescriptor {
                    .index_id = index_entry->id(),
                    .collection_id = index_entry->collection_id(),
                    .column_id = *column_id,
                    .column_ordinal = column->ordinal(),
                    .key_type = column->type(),
                    .kind = (*backend)->kind(),
                    .unique = index_entry->unique(),
                }, std::move(*backend)};

                restored.stores_by_id_.emplace(index_entry->id(), std::move(store));
                restored.indexes_by_collection_[index_entry->collection_id()].push_back(index_entry->id());
            }
        }
    }

    *this = std::move(restored);
    return {};
}

std::expected<void, IndexError> IndexEngine::reload_collection(
    const meta::CatalogView & catalog,
    const storage::StorageEngine & storage,
    common::CollectionId collection_id
)
{
    if (!storage.contains_collection(collection_id)) {
        return std::unexpected(make_error(IndexErrorCode::StorageError, "Index collection is absent from storage"));
    }

    auto collection_schema = storage::load_collection_schema(catalog, collection_id);
    if (!collection_schema) {
        return std::unexpected(make_error(IndexErrorCode::InvalidIndexColumn, collection_schema.error().message()));
    }

    IndexEngine restored {data_directory_, *filesystem_};
    for (const auto * index_entry : catalog.list_indexes(collection_id)) {
        if (index_entry == nullptr) continue;
        const auto column_id = index_entry->column_id();
        if (!column_id) {
            return std::unexpected(make_error(IndexErrorCode::InvalidIndexColumn, "Index has no columns"));
        }
        const auto * column = collection_schema->find_column(*column_id);
        if (column == nullptr || column->type().id == common::LogicalTypeId::Vector) {
            return std::unexpected(make_error(IndexErrorCode::InvalidIndexColumn, "Indexed column is invalid"));
        }
        auto backend = restored.restore_backend(*index_entry, column->type());
        if (!backend) return std::unexpected(std::move(backend.error()));

        restored.stores_by_id_.emplace(index_entry->id(), IndexStore {IndexDescriptor {
            .index_id = index_entry->id(),
            .collection_id = index_entry->collection_id(),
            .column_id = *column_id,
            .column_ordinal = column->ordinal(),
            .key_type = column->type(),
            .kind = (*backend)->kind(),
            .unique = index_entry->unique(),
        }, std::move(*backend)});
        restored.indexes_by_collection_[collection_id].push_back(index_entry->id());
    }

    if (const auto current = indexes_by_collection_.find(collection_id); current != indexes_by_collection_.end()) {
        for (const auto index_id : current->second) stores_by_id_.erase(index_id);
        indexes_by_collection_.erase(current);
    }
    for (auto & [index_id, store] : restored.stores_by_id_) {
        stores_by_id_.emplace(index_id, std::move(store));
    }
    if (const auto ids = restored.indexes_by_collection_.find(collection_id);
        ids != restored.indexes_by_collection_.end()) {
        indexes_by_collection_.emplace(collection_id, std::move(ids->second));
    }
    return {};
}

std::expected<IndexKeyBindings, IndexError> IndexEngine::prepare_insert(
    common::CollectionId collection_id,
    const common::RecordData & record_data
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
    const auto rollback_inserted = [&]() {
        std::string failure;
        for (auto it = inserted_bindings.rbegin(); it != inserted_bindings.rend(); ++it) {
            auto * rollback_index = find_store(it->index_id);
            if (rollback_index == nullptr) {
                failure += " rollback index disappeared;";
                continue;
            }
            auto rolled_back = rollback_index->erase(it->key, record_id);
            if (!rolled_back.has_value()) {
                failure += " rollback erase failed: " + rolled_back.error().message() + ';';
            }
        }
        return failure;
    };

    for (const auto & binding : bindings) {
        auto * store = find_store(binding.index_id);
        if (store == nullptr) {
            const auto rollback_failure = rollback_inserted();
            if (!rollback_failure.empty()) {
                return std::unexpected(make_error(
                    IndexErrorCode::StorageError,
                    "Index disappeared during insert;" + rollback_failure
                ));
            }
            return std::unexpected(make_error(IndexErrorCode::IndexNotFound, "Index not found"));
        }
        auto inserted = store->insert(binding.key, record_id);
        if (!inserted.has_value()) {
            const auto rollback_failure = rollback_inserted();
            if (!rollback_failure.empty()) {
                return std::unexpected(make_error(
                    IndexErrorCode::StorageError,
                    inserted.error().message() + ';' + rollback_failure
                ));
            }
            return std::unexpected(std::move(inserted.error()));
        }
        inserted_bindings.push_back(binding);
    }
    return {};
}

std::expected<IndexUpdateBindings, IndexError> IndexEngine::prepare_update(
    common::CollectionId collection_id,
    const common::RecordData & old_record_data,
    const common::RecordData & new_record_data
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
            .old_key = std::move(*old_key),
            .new_key = std::move(*new_key),
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
    const auto rollback_applied = [&]() {
        std::string failure;
        for (auto it = applied_bindings.rbegin(); it != applied_bindings.rend(); ++it) {
            auto * rollback_index = find_store(it->index_id);
            if (rollback_index == nullptr) {
                failure += " rollback index disappeared;";
                continue;
            }
            if (it->new_key.has_value()) {
                auto erased = rollback_index->erase(*it->new_key, record_id);
                if (!erased.has_value()) {
                    failure += " rollback new-key erase failed: " + erased.error().message() + ';';
                }
            }
            if (it->old_key.has_value()) {
                auto inserted = rollback_index->insert(*it->old_key, record_id);
                if (!inserted.has_value()) {
                    failure += " rollback old-key insert failed: " + inserted.error().message() + ';';
                }
            }
        }
        return failure;
    };

    for (const auto & binding : bindings) {
        auto * store = find_store(binding.index_id);
        if (store == nullptr) {
            const auto rollback_failure = rollback_applied();
            if (!rollback_failure.empty()) {
                return std::unexpected(make_error(
                    IndexErrorCode::StorageError,
                    "Index disappeared during update;" + rollback_failure
                ));
            }
            return std::unexpected(make_error(IndexErrorCode::IndexNotFound, "Index not found"));
        }

        if (!binding.key_changed) {
            continue;
        }

        if (binding.new_key.has_value()) {
            auto inserted = store->insert(*binding.new_key, record_id);
            if (!inserted.has_value()) {
                const auto rollback_failure = rollback_applied();
                if (!rollback_failure.empty()) {
                    return std::unexpected(make_error(
                        IndexErrorCode::StorageError,
                        inserted.error().message() + ';' + rollback_failure
                    ));
                }
                return std::unexpected(std::move(inserted.error()));
            }
        }

        if (binding.old_key.has_value()) {
            auto erased = store->erase(*binding.old_key, record_id);
            if (!erased.has_value()) {
                std::string rollback_failure;
                if (binding.new_key.has_value()) {
                    auto removed_new = store->erase(*binding.new_key, record_id);
                    if (!removed_new.has_value()) {
                        rollback_failure += " current new-key erase failed: "
                            + removed_new.error().message() + ';';
                    }
                }
                rollback_failure += rollback_applied();
                if (!rollback_failure.empty()) {
                    return std::unexpected(make_error(
                        IndexErrorCode::StorageError,
                        erased.error().message() + ';' + rollback_failure
                    ));
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
    const common::RecordData & old_record_data
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
    const auto rollback_erased = [&]() {
        std::string failure;
        for (auto it = erased_bindings.rbegin(); it != erased_bindings.rend(); ++it) {
            auto * rollback_index = find_store(it->index_id);
            if (rollback_index == nullptr) {
                failure += " rollback index disappeared;";
                continue;
            }
            auto rolled_back = rollback_index->insert(it->key, record_id);
            if (!rolled_back.has_value()) {
                failure += " rollback insert failed: " + rolled_back.error().message() + ';';
            }
        }
        return failure;
    };

    for (const auto & binding : bindings) {
        auto * store = find_store(binding.index_id);
        if (store == nullptr) {
            const auto rollback_failure = rollback_erased();
            if (!rollback_failure.empty()) {
                return std::unexpected(make_error(
                    IndexErrorCode::StorageError,
                    "Index disappeared during delete;" + rollback_failure
                ));
            }
            return std::unexpected(make_error(IndexErrorCode::IndexNotFound, "Index not found"));
        }
        auto erased = store->erase(binding.key, record_id);
        if (!erased.has_value()) {
            const auto rollback_failure = rollback_erased();
            if (!rollback_failure.empty()) {
                return std::unexpected(make_error(
                    IndexErrorCode::StorageError,
                    erased.error().message() + ';' + rollback_failure
                ));
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

std::expected<std::unique_ptr<ScalarIndexCursor>, IndexError>
IndexEngine::scan_range_cursor(
    common::IndexId index_id,
    const IndexRange & range
) const
{
    const auto * store = find_store(index_id);
    if (store == nullptr) {
        return std::unexpected(make_error(IndexErrorCode::IndexNotFound, "Index not found"));
    }
    return store->scan_range_cursor(range);
}

void IndexEngine::clear() noexcept
{
    stores_by_id_.clear();
    indexes_by_collection_.clear();
}

} // namespace litedb::core::index
