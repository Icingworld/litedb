#include "core/index/index_manager.hpp"

#include <algorithm>
#include <utility>

#include "core/meta/meta_engine.hpp"
#include "core/index/btree_index.hpp"
#include "core/index/hash_index.hpp"
#include "core/schema/schema_loader.hpp"
#include "core/storage/storage_engine.hpp"

namespace litedb::core::index
{

namespace
{

IndexError make_error(IndexErrorCode code, std::string message)
{
    return IndexError {code, std::move(message)};
}

} // namespace

std::unique_ptr<ScalarIndex> IndexManager::make_index(meta::entry::IndexKind index_kind)
{
    switch (index_kind) {
    case meta::entry::IndexKind::Hash:
        return std::make_unique<HashIndex>();
    case meta::entry::IndexKind::BTree:
        return std::make_unique<BTreeIndex>();
    }
    return nullptr;
}

std::expected<std::optional<ScalarIndexKey>, IndexError> IndexManager::make_key_from_record(
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

std::expected<void, IndexError> IndexManager::validate_key_type(
    const ManagedIndex & managed_index,
    const ScalarIndexKey & key
)
{
    if (!key.value().matches_type(managed_index.key_type)) {
        return std::unexpected(make_error(
            IndexErrorCode::KeyTypeMismatch,
            "Index key type does not match indexed column type"
        ));
    }
    return {};
}

std::expected<void, IndexError> IndexManager::validate_unique_key(
    const ManagedIndex & managed_index,
    const ScalarIndexKey & key
)
{
    if (!managed_index.unique) {
        return {};
    }

    auto existing = managed_index.index->find_equal(key);
    if (!existing.has_value()) {
        return std::unexpected(std::move(existing.error()));
    }
    if (!existing->empty()) {
        return std::unexpected(make_error(IndexErrorCode::DuplicateKey, "Unique index key already exists"));
    }
    return {};
}

ManagedIndexView IndexManager::make_view(const ManagedIndex & managed_index) const noexcept
{
    return ManagedIndexView {
        .index_id = managed_index.index_id,
        .collection_id = managed_index.collection_id,
        .column_id = managed_index.column_id,
        .column_ordinal = managed_index.column_ordinal,
        .key_type = managed_index.key_type,
        .kind = managed_index.kind,
        .unique = managed_index.unique,
        .entry_count = managed_index.index->size(),
    };
}

const IndexManager::ManagedIndex * IndexManager::find_managed_index(common::IndexId index_id) const noexcept
{
    const auto it = indexes_by_id_.find(index_id);
    if (it == indexes_by_id_.end()) {
        return nullptr;
    }
    return &it->second;
}

IndexManager::ManagedIndex * IndexManager::find_managed_index(common::IndexId index_id) noexcept
{
    const auto it = indexes_by_id_.find(index_id);
    if (it == indexes_by_id_.end()) {
        return nullptr;
    }
    return &it->second;
}

std::vector<const IndexManager::ManagedIndex *> IndexManager::list_managed_indexes(
    common::CollectionId collection_id
) const
{
    std::vector<const ManagedIndex *> indexes;
    const auto it = indexes_by_collection_.find(collection_id);
    if (it == indexes_by_collection_.end()) {
        return indexes;
    }

    indexes.reserve(it->second.size());
    for (const auto index_id : it->second) {
        if (const auto * managed_index = find_managed_index(index_id); managed_index != nullptr) {
            indexes.push_back(managed_index);
        }
    }
    return indexes;
}

std::expected<void, IndexError> IndexManager::build_index_from_storage(
    ManagedIndex & managed_index,
    const storage::StorageEngine & storage
) const
{
    auto cursor = storage.scan(managed_index.collection_id);
    if (!cursor) return std::unexpected(make_error(IndexErrorCode::StorageError, cursor.error().message));
    while (true) {
        auto next = cursor->next();
        if (!next) return std::unexpected(make_error(IndexErrorCode::StorageError, next.error().message));
        if (!*next) break;
        const auto & record = **next;
        auto key = make_key_from_record(record.data, managed_index.column_ordinal, managed_index.key_type);
        if (!key.has_value()) {
            return std::unexpected(std::move(key.error()));
        }
        if (!key->has_value()) {
            continue;
        }

        auto unique = validate_unique_key(managed_index, key->value());
        if (!unique.has_value()) {
            return std::unexpected(std::move(unique.error()));
        }

        auto inserted = managed_index.index->insert(key->value(), record.record_id);
        if (!inserted.has_value()) {
            return std::unexpected(std::move(inserted.error()));
        }
    }
    return {};
}

std::expected<void, IndexError> IndexManager::create_index(
    const meta::entry::IndexEntry & index_entry,
    const schema::CollectionSchema & collection_schema,
    const storage::StorageEngine & storage
)
{
    if (find_managed_index(index_entry.id()) != nullptr) {
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

    auto index = make_index(index_entry.kind());
    if (index == nullptr) {
        return std::unexpected(make_error(IndexErrorCode::InvalidIndexColumn, "Unsupported index kind"));
    }

    ManagedIndex managed_index {
        .index_id = index_entry.id(),
        .collection_id = index_entry.collection_id(),
        .column_id = column_id.value(),
        .column_ordinal = column->ordinal(),
        .key_type = column->type(),
        .kind = index->kind(),
        .unique = index_entry.unique(),
        .index = std::move(index),
    };

    auto built = build_index_from_storage(managed_index, storage);
    if (!built.has_value()) {
        return std::unexpected(std::move(built.error()));
    }

    const auto index_id = managed_index.index_id;
    const auto collection_id = managed_index.collection_id;
    indexes_by_id_.emplace(index_id, std::move(managed_index));
    indexes_by_collection_[collection_id].push_back(index_id);
    return {};
}

std::expected<void, IndexError> IndexManager::drop_index(common::IndexId index_id)
{
    const auto it = indexes_by_id_.find(index_id);
    if (it == indexes_by_id_.end()) {
        return std::unexpected(make_error(IndexErrorCode::IndexNotFound, "Index not found"));
    }

    const auto collection_id = it->second.collection_id;
    indexes_by_id_.erase(it);

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

void IndexManager::drop_collection_indexes(common::CollectionId collection_id)
{
    const auto it = indexes_by_collection_.find(collection_id);
    if (it == indexes_by_collection_.end()) {
        return;
    }

    for (const auto index_id : it->second) {
        indexes_by_id_.erase(index_id);
    }
    indexes_by_collection_.erase(it);
}

std::expected<void, IndexError> IndexManager::rebuild_all(
    const meta::MetaEngine & catalog,
    const storage::StorageEngine & storage
)
{
    IndexManager rebuilt;

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

std::expected<IndexKeyBindings, IndexError> IndexManager::prepare_insert(
    common::CollectionId collection_id,
    const schema::RecordData & record_data
) const
{
    IndexKeyBindings bindings;
    for (const auto * managed_index : list_managed_indexes(collection_id)) {
        auto key = make_key_from_record(record_data, managed_index->column_ordinal, managed_index->key_type);
        if (!key.has_value()) {
            return std::unexpected(std::move(key.error()));
        }
        if (!key->has_value()) {
            continue;
        }

        auto unique = validate_unique_key(*managed_index, key->value());
        if (!unique.has_value()) {
            return std::unexpected(std::move(unique.error()));
        }

        bindings.push_back(IndexKeyBinding {
            .index_id = managed_index->index_id,
            .key = std::move(key->value()),
        });
    }
    return bindings;
}

std::expected<void, IndexError> IndexManager::on_insert(
    common::RecordId record_id,
    const IndexKeyBindings & bindings
)
{
    IndexKeyBindings inserted_bindings;
    inserted_bindings.reserve(bindings.size());

    for (const auto & binding : bindings) {
        auto * managed_index = find_managed_index(binding.index_id);
        if (managed_index == nullptr) {
            return std::unexpected(make_error(IndexErrorCode::IndexNotFound, "Index not found"));
        }
        auto valid_key = validate_key_type(*managed_index, binding.key);
        if (!valid_key.has_value()) {
            return std::unexpected(std::move(valid_key.error()));
        }

        auto inserted = managed_index->index->insert(binding.key, record_id);
        if (!inserted.has_value()) {
            for (auto it = inserted_bindings.rbegin(); it != inserted_bindings.rend(); ++it) {
                if (auto * rollback_index = find_managed_index(it->index_id); rollback_index != nullptr) {
                    (void) rollback_index->index->erase(it->key, record_id);
                }
            }
            return std::unexpected(std::move(inserted.error()));
        }
        inserted_bindings.push_back(binding);
    }
    return {};
}

std::expected<IndexUpdateBindings, IndexError> IndexManager::prepare_update(
    common::CollectionId collection_id,
    const schema::RecordData & old_record_data,
    const schema::RecordData & new_record_data
) const
{
    IndexUpdateBindings bindings;
    for (const auto * managed_index : list_managed_indexes(collection_id)) {
        auto old_key = make_key_from_record(old_record_data, managed_index->column_ordinal, managed_index->key_type);
        if (!old_key.has_value()) {
            return std::unexpected(std::move(old_key.error()));
        }

        auto new_key = make_key_from_record(new_record_data, managed_index->column_ordinal, managed_index->key_type);
        if (!new_key.has_value()) {
            return std::unexpected(std::move(new_key.error()));
        }

        IndexUpdateBinding binding {
            .index_id = managed_index->index_id,
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
            auto unique = validate_unique_key(*managed_index, binding.new_key.value());
            if (!unique.has_value()) {
                return std::unexpected(std::move(unique.error()));
            }
        }

        bindings.push_back(std::move(binding));
    }
    return bindings;
}

std::expected<void, IndexError> IndexManager::on_update(
    common::RecordId record_id,
    const IndexUpdateBindings & bindings
)
{
    std::vector<IndexUpdateBinding> applied_bindings;

    for (const auto & binding : bindings) {
        auto * managed_index = find_managed_index(binding.index_id);
        if (managed_index == nullptr) {
            return std::unexpected(make_error(IndexErrorCode::IndexNotFound, "Index not found"));
        }

        if (!binding.key_changed) {
            continue;
        }

        if (binding.old_key.has_value()) {
            auto valid_key = validate_key_type(*managed_index, binding.old_key.value());
            if (!valid_key.has_value()) {
                return std::unexpected(std::move(valid_key.error()));
            }
        }
        if (binding.new_key.has_value()) {
            auto valid_key = validate_key_type(*managed_index, binding.new_key.value());
            if (!valid_key.has_value()) {
                return std::unexpected(std::move(valid_key.error()));
            }
        }

        if (binding.new_key.has_value()) {
            auto inserted = managed_index->index->insert(binding.new_key.value(), record_id);
            if (!inserted.has_value()) {
                for (auto it = applied_bindings.rbegin(); it != applied_bindings.rend(); ++it) {
                    if (auto * rollback_index = find_managed_index(it->index_id); rollback_index != nullptr) {
                        if (it->new_key.has_value()) {
                            (void) rollback_index->index->erase(it->new_key.value(), record_id);
                        }
                        if (it->old_key.has_value()) {
                            (void) rollback_index->index->insert(it->old_key.value(), record_id);
                        }
                    }
                }
                return std::unexpected(std::move(inserted.error()));
            }
        }

        if (binding.old_key.has_value()) {
            auto erased = managed_index->index->erase(binding.old_key.value(), record_id);
            if (!erased.has_value()) {
                if (binding.new_key.has_value()) {
                    (void) managed_index->index->erase(binding.new_key.value(), record_id);
                }
                for (auto it = applied_bindings.rbegin(); it != applied_bindings.rend(); ++it) {
                    if (auto * rollback_index = find_managed_index(it->index_id); rollback_index != nullptr) {
                        if (it->new_key.has_value()) {
                            (void) rollback_index->index->erase(it->new_key.value(), record_id);
                        }
                        if (it->old_key.has_value()) {
                            (void) rollback_index->index->insert(it->old_key.value(), record_id);
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

std::expected<IndexKeyBindings, IndexError> IndexManager::prepare_delete(
    common::CollectionId collection_id,
    const schema::RecordData & old_record_data
) const
{
    IndexKeyBindings bindings;
    for (const auto * managed_index : list_managed_indexes(collection_id)) {
        auto key = make_key_from_record(old_record_data, managed_index->column_ordinal, managed_index->key_type);
        if (!key.has_value()) {
            return std::unexpected(std::move(key.error()));
        }
        if (!key->has_value()) {
            continue;
        }

        bindings.push_back(IndexKeyBinding {
            .index_id = managed_index->index_id,
            .key = std::move(key->value()),
        });
    }
    return bindings;
}

std::expected<void, IndexError> IndexManager::on_delete(
    common::RecordId record_id,
    const IndexKeyBindings & bindings
)
{
    IndexKeyBindings erased_bindings;
    erased_bindings.reserve(bindings.size());

    for (const auto & binding : bindings) {
        auto * managed_index = find_managed_index(binding.index_id);
        if (managed_index == nullptr) {
            return std::unexpected(make_error(IndexErrorCode::IndexNotFound, "Index not found"));
        }
        auto valid_key = validate_key_type(*managed_index, binding.key);
        if (!valid_key.has_value()) {
            return std::unexpected(std::move(valid_key.error()));
        }

        auto erased = managed_index->index->erase(binding.key, record_id);
        if (!erased.has_value()) {
            for (auto it = erased_bindings.rbegin(); it != erased_bindings.rend(); ++it) {
                if (auto * rollback_index = find_managed_index(it->index_id); rollback_index != nullptr) {
                    (void) rollback_index->index->insert(it->key, record_id);
                }
            }
            return std::unexpected(std::move(erased.error()));
        }
        erased_bindings.push_back(binding);
    }
    return {};
}

std::optional<ManagedIndexView> IndexManager::find_index(common::IndexId index_id) const noexcept
{
    const auto * managed_index = find_managed_index(index_id);
    if (managed_index == nullptr) {
        return std::nullopt;
    }
    return make_view(*managed_index);
}

std::vector<ManagedIndexView> IndexManager::list_indexes(common::CollectionId collection_id) const
{
    std::vector<ManagedIndexView> views;
    for (const auto * managed_index : list_managed_indexes(collection_id)) {
        views.push_back(make_view(*managed_index));
    }
    return views;
}

std::vector<ManagedIndexView> IndexManager::find_indexes_for_column(
    common::CollectionId collection_id,
    common::ColumnId column_id
) const
{
    std::vector<ManagedIndexView> views;
    for (const auto * managed_index : list_managed_indexes(collection_id)) {
        if (managed_index->column_id == column_id) {
            views.push_back(make_view(*managed_index));
        }
    }
    return views;
}

std::expected<std::vector<common::RecordId>, IndexError> IndexManager::find_equal(
    common::IndexId index_id,
    const ScalarIndexKey & key
) const
{
    const auto * managed_index = find_managed_index(index_id);
    if (managed_index == nullptr) {
        return std::unexpected(make_error(IndexErrorCode::IndexNotFound, "Index not found"));
    }
    auto valid_key = validate_key_type(*managed_index, key);
    if (!valid_key.has_value()) {
        return std::unexpected(std::move(valid_key.error()));
    }
    return managed_index->index->find_equal(key);
}

std::expected<std::vector<common::RecordId>, IndexError> IndexManager::scan_range(
    common::IndexId index_id,
    const IndexRange & range
) const
{
    const auto * managed_index = find_managed_index(index_id);
    if (managed_index == nullptr) {
        return std::unexpected(make_error(IndexErrorCode::IndexNotFound, "Index not found"));
    }
    const auto * ordered_index = dynamic_cast<const OrderedScalarIndex *>(managed_index->index.get());
    if (ordered_index == nullptr) {
        return std::unexpected(make_error(
            IndexErrorCode::UnsupportedRangeScan,
            "Index does not support range scans"
        ));
    }
    if (range.lower().has_value()) {
        auto valid_key = validate_key_type(*managed_index, range.lower()->key);
        if (!valid_key.has_value()) {
            return std::unexpected(std::move(valid_key.error()));
        }
    }
    if (range.upper().has_value()) {
        auto valid_key = validate_key_type(*managed_index, range.upper()->key);
        if (!valid_key.has_value()) {
            return std::unexpected(std::move(valid_key.error()));
        }
    }
    return ordered_index->scan_range(range);
}

void IndexManager::clear() noexcept
{
    indexes_by_id_.clear();
    indexes_by_collection_.clear();
}

} // namespace litedb::core::index
