#include "core/persistence/persistent_collection_storage.hpp"

#include <algorithm>
#include <utility>

namespace litedb::core::persistence
{

namespace
{

storage::StorageError make_error(storage::StorageErrorCode code, std::string message)
{
    return storage::StorageError {code, std::move(message)};
}

class PersistentRecordCursor final : public storage::RecordCursor
{
public:
    explicit PersistentRecordCursor(std::vector<schema::Record> records)
        : records_(std::move(records))
    {
    }

    std::optional<schema::Record> next() override
    {
        if (position_ >= records_.size()) {
            return std::nullopt;
        }
        return records_[position_++];
    }

private:
    std::vector<schema::Record> records_;
    std::size_t position_ {0};
};

} // namespace

std::expected<std::unique_ptr<PersistentCollectionStorage>, storage::StorageError> PersistentCollectionStorage::open(
    schema::CollectionSchema collection_schema,
    RowLog row_log
)
{
    auto storage = std::make_unique<PersistentCollectionStorage>(std::move(collection_schema), std::move(row_log));
    auto replayed = storage->replay();
    if (!replayed.has_value()) {
        return std::unexpected(std::move(replayed.error()));
    }
    return storage;
}

PersistentCollectionStorage::PersistentCollectionStorage(schema::CollectionSchema collection_schema, RowLog row_log)
    : collection_schema_(std::move(collection_schema))
    , row_log_(std::move(row_log))
{
}

const schema::CollectionSchema & PersistentCollectionStorage::collection_schema() const noexcept
{
    return collection_schema_;
}

std::expected<schema::Record, storage::StorageError> PersistentCollectionStorage::get(common::RecordId record_id) const
{
    const auto it = records_.find(record_id);
    if (it == records_.end()) {
        return std::unexpected(make_error(storage::StorageErrorCode::RecordNotFound, "Record not found"));
    }
    return schema::Record {.record_id = record_id, .data = it->second};
}

std::expected<common::RecordId, storage::StorageError> PersistentCollectionStorage::insert(schema::RecordData record_data)
{
    auto validation = validate_record(record_data);
    if (!validation.has_value()) {
        return std::unexpected(std::move(validation.error()));
    }

    const auto record_id = next_record_id_;
    auto appended = row_log_.append_insert(record_id, record_data);
    if (!appended.has_value()) {
        return std::unexpected(std::move(appended.error()));
    }

    ++next_record_id_;
    apply_upsert(record_id, std::move(record_data));
    return record_id;
}

std::expected<void, storage::StorageError> PersistentCollectionStorage::update(
    common::RecordId record_id,
    schema::RecordData record_data
)
{
    if (!records_.contains(record_id)) {
        return std::unexpected(make_error(storage::StorageErrorCode::RecordNotFound, "Record not found"));
    }

    auto validation = validate_record(record_data);
    if (!validation.has_value()) {
        return std::unexpected(std::move(validation.error()));
    }

    auto appended = row_log_.append_update(record_id, record_data);
    if (!appended.has_value()) {
        return std::unexpected(std::move(appended.error()));
    }

    apply_upsert(record_id, std::move(record_data));
    return {};
}

std::expected<void, storage::StorageError> PersistentCollectionStorage::erase(common::RecordId record_id)
{
    if (!records_.contains(record_id)) {
        return std::unexpected(make_error(storage::StorageErrorCode::RecordNotFound, "Record not found"));
    }

    auto appended = row_log_.append_delete(record_id);
    if (!appended.has_value()) {
        return std::unexpected(std::move(appended.error()));
    }

    apply_delete(record_id);
    return {};
}

std::unique_ptr<storage::RecordCursor> PersistentCollectionStorage::scan() const
{
    std::vector<schema::Record> records;
    records.reserve(record_ids_.size());
    for (const auto record_id : record_ids_) {
        const auto it = records_.find(record_id);
        if (it != records_.end()) {
            records.push_back(schema::Record {
                .record_id = record_id,
                .data = it->second,
            });
        }
    }
    return std::make_unique<PersistentRecordCursor>(std::move(records));
}

std::expected<void, storage::StorageError> PersistentCollectionStorage::replay()
{
    auto replayed = row_log_.replay_or_create();
    if (!replayed.has_value()) {
        return std::unexpected(std::move(replayed.error()));
    }

    next_record_id_ = replayed->next_record_id;
    for (auto & record : replayed->records) {
        switch (record.operation) {
        case RowLogOperation::Insert:
        case RowLogOperation::Update: {
            auto validation = validate_record(record.data);
            if (!validation.has_value()) {
                return std::unexpected(std::move(validation.error()));
            }
            apply_upsert(record.record_id, std::move(record.data));
            break;
        }
        case RowLogOperation::Delete:
            apply_delete(record.record_id);
            break;
        }
    }
    return {};
}

std::expected<void, storage::StorageError> PersistentCollectionStorage::validate_record(
    const schema::RecordData & record_data
) const
{
    const auto & columns = collection_schema_.columns();
    if (record_data.values.size() != columns.size()) {
        return std::unexpected(make_error(storage::StorageErrorCode::ValueCountMismatch, "Record value count does not match schema"));
    }

    for (std::size_t index = 0; index < columns.size(); ++index) {
        const auto & column = columns[index];
        const auto & value = record_data.values[index];
        if (value.is_null() && !column.nullable()) {
            return std::unexpected(make_error(storage::StorageErrorCode::NullConstraintViolation, "Column cannot be null: " + column.column_name()));
        }
        if (!value.matches_type(column.type())) {
            return std::unexpected(make_error(storage::StorageErrorCode::TypeMismatch, "Value type does not match column: " + column.column_name()));
        }
        if (value.is_null()) {
            continue;
        }
        if (column.type().id == common::LogicalTypeId::Varchar && column.type().parameter.has_value()) {
            const auto & text = std::get<std::string>(value.data());
            if (text.size() > column.type().parameter.value()) {
                return std::unexpected(make_error(storage::StorageErrorCode::TypeMismatch, "VARCHAR value exceeds declared length: " + column.column_name()));
            }
        }
        if (column.type().id == common::LogicalTypeId::Vector && column.type().parameter.has_value()) {
            const auto & vector = std::get<schema::VectorValue>(value.data());
            if (vector.size() != column.type().parameter.value()) {
                return std::unexpected(make_error(storage::StorageErrorCode::TypeMismatch, "VECTOR value dimension mismatch: " + column.column_name()));
            }
        }
    }
    return {};
}

void PersistentCollectionStorage::apply_upsert(common::RecordId record_id, schema::RecordData record_data)
{
    if (!records_.contains(record_id)) {
        record_ids_.push_back(record_id);
    }
    records_[record_id] = std::move(record_data);
}

void PersistentCollectionStorage::apply_delete(common::RecordId record_id)
{
    records_.erase(record_id);
    std::erase(record_ids_, record_id);
}

} // namespace litedb::core::persistence
