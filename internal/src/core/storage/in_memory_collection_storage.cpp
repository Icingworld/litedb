#include "core/storage/in_memory_collection_storage.hpp"

#include <utility>

namespace litedb::core::storage
{

namespace
{

StorageError make_error(StorageErrorCode code, std::string message)
{
    return StorageError {code, std::move(message)};
}

/**
 * @brief 内存记录游标
 */
class InMemoryRecordCursor final : public RecordCursor
{
public:
    explicit InMemoryRecordCursor(std::vector<schema::Record> records)
        : records_(std::move(records))
    {
    }

public:
    std::optional<schema::Record> next() override
    {
        if (position_ >= records_.size()) {
            return std::nullopt;
        }
        return records_[position_++];
    }

private:
    std::vector<schema::Record> records_;   ///< 记录
    std::size_t position_ {0};              ///< 位置
};

} // namespace

InMemoryCollectionStorage::InMemoryCollectionStorage(schema::CollectionSchema collection_schema)
    : collection_schema_(std::move(collection_schema))
{
}

const schema::CollectionSchema & InMemoryCollectionStorage::collection_schema() const noexcept
{
    return collection_schema_;
}

std::expected<schema::Record, StorageError> InMemoryCollectionStorage::get(common::RecordId record_id) const
{
    const auto it = records_.find(record_id);
    if (it == records_.end()) {
        return std::unexpected(make_error(StorageErrorCode::RecordNotFound, "Record not found"));
    }
    return schema::Record {.record_id = record_id, .data = it->second};
}

std::expected<common::RecordId, StorageError> InMemoryCollectionStorage::insert(schema::RecordData record_data)
{
    auto validation = validate_record(record_data);
    if (!validation.has_value()) {
        return std::unexpected(std::move(validation.error()));
    }

    const auto record_id = next_record_id_++;
    record_ids_.push_back(record_id);
    records_.emplace(record_id, std::move(record_data));
    return record_id;
}

std::expected<void, StorageError> InMemoryCollectionStorage::update(
    common::RecordId record_id,
    schema::RecordData record_data
)
{
    auto it = records_.find(record_id);
    if (it == records_.end()) {
        return std::unexpected(make_error(StorageErrorCode::RecordNotFound, "Record not found"));
    }

    auto validation = validate_record(record_data);
    if (!validation.has_value()) {
        return std::unexpected(std::move(validation.error()));
    }

    it->second = std::move(record_data);
    return {};
}

std::expected<void, StorageError> InMemoryCollectionStorage::erase(common::RecordId record_id)
{
    const auto erased = records_.erase(record_id);
    if (erased == 0) {
        return std::unexpected(make_error(StorageErrorCode::RecordNotFound, "Record not found"));
    }

    std::erase(record_ids_, record_id);
    return {};
}

std::unique_ptr<RecordCursor> InMemoryCollectionStorage::scan() const
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

    return std::make_unique<InMemoryRecordCursor>(std::move(records));
}

std::expected<void, StorageError> InMemoryCollectionStorage::validate_record(
    const schema::RecordData & record_data
) const
{
    const auto & columns = collection_schema_.columns();
    if (record_data.values.size() != columns.size()) {
        return std::unexpected(make_error(StorageErrorCode::ValueCountMismatch, "Record value count does not match schema"));
    }

    for (std::size_t index = 0; index < columns.size(); ++index) {
        const auto & column = columns[index];
        const auto & value = record_data.values[index];

        if (value.is_null() && !column.nullable()) {
            return std::unexpected(make_error(StorageErrorCode::NullConstraintViolation, "Column cannot be null: " + column.column_name()));
        }

        if (!value.matches_type(column.type())) {
            return std::unexpected(make_error(StorageErrorCode::TypeMismatch, "Value type does not match column: " + column.column_name()));
        }
    }

    return {};
}

} // namespace litedb::core::storage
