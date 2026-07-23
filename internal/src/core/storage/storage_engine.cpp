#include "core/storage/storage_engine.hpp"

#include <string>
#include <utility>

#include "core/common/logical_type.hpp"
#include "core/io/binary_io.hpp"
#include "core/io/buffer_byte_writer.hpp"
#include "core/storage/storage_store.hpp"

namespace litedb::core::storage
{

namespace
{

/**
 * @brief 创建存储错误
 * @param code 错误码
 * @param message 错误消息
 * @return 存储错误
 */
[[nodiscard]]
StorageError make_error(StorageErrorCode code, std::string message)
{
    return {code, std::move(message), std::nullopt};
}

} // namespace

StorageEngine::StorageEngine(std::filesystem::path data_directory, filesystem::FileSystem & filesystem) noexcept
    : data_directory_(std::move(data_directory))
    , filesystem_(&filesystem)
{
}

StorageEngine::~StorageEngine() = default;

StorageEngine::StorageEngine(StorageEngine &&) noexcept = default;

StorageEngine & StorageEngine::operator=(StorageEngine &&) noexcept = default;

std::filesystem::path StorageEngine::store_path(common::CollectionId collection_id) const
{
    return data_directory_ / "collections" / (std::to_string(collection_id) + ".store");
}

std::expected<void, StorageError> StorageEngine::create_collection(schema::CollectionSchema schema)
{
    const auto id = schema.collection_id();
    if (collections_.contains(id)) {
        return std::unexpected(make_error(StorageErrorCode::CollectionAlreadyExists, "Collection already exists"));
    }
    if (filesystem_ == nullptr) {
        return std::unexpected(StorageError {
            StorageErrorCode::StoreError,
            "Storage engine is not configured",
            StorageStoreErrorCode::InvalidStoreState,
        });
    }
    auto exists = filesystem_->exists(store_path(id));
    if (!exists) {
        return std::unexpected(StorageError {
            StorageErrorCode::StoreError,
            std::move(exists.error().message),
            StorageStoreErrorCode::FileSystemError,
        });
    }
    if (*exists) {
        return std::unexpected(make_error(StorageErrorCode::CollectionStoreAlreadyExists, "Collection store already exists"));
    }
    auto created = StorageStore::create(store_path(id), id, *filesystem_);
    if (!created) {
        return std::unexpected(from_storage_store_error(std::move(created.error())));
    }
    auto store = std::move(*created);
    collections_.emplace(id, CollectionState {std::move(schema), std::move(store)});
    return {};
}

std::expected<void, StorageError> StorageEngine::open_collection(schema::CollectionSchema schema)
{
    const auto id = schema.collection_id();
    if (collections_.contains(id)) {
        return std::unexpected(make_error(StorageErrorCode::CollectionAlreadyExists, "Collection already exists"));
    }
    if (filesystem_ == nullptr) {
        return std::unexpected(StorageError {
            StorageErrorCode::StoreError,
            "Storage engine is not configured",
            StorageStoreErrorCode::InvalidStoreState,
        });
    }
    auto exists = filesystem_->exists(store_path(id));
    if (!exists) {
        return std::unexpected(StorageError {
            StorageErrorCode::StoreError,
            std::move(exists.error().message),
            StorageStoreErrorCode::FileSystemError,
        });
    }
    if (!*exists) {
        return std::unexpected(make_error(StorageErrorCode::CollectionStoreNotFound, "Collection store not found"));
    }
    auto opened = StorageStore::open(store_path(id), id, *filesystem_);
    if (!opened) {
        return std::unexpected(from_storage_store_error(std::move(opened.error())));
    }
    collections_.emplace(id, CollectionState {std::move(schema), std::move(*opened)});
    return {};
}

std::expected<void, StorageError> StorageEngine::reload_collection(schema::CollectionSchema schema)
{
    const auto id = schema.collection_id();
    if (filesystem_ == nullptr) {
        return std::unexpected(StorageError {
            StorageErrorCode::StoreError,
            "Storage engine is not configured",
            StorageStoreErrorCode::InvalidStoreState,
        });
    }
    auto opened = StorageStore::open(store_path(id), id, *filesystem_);
    if (!opened) {
        return std::unexpected(from_storage_store_error(std::move(opened.error())));
    }
    collections_.insert_or_assign(id, CollectionState {std::move(schema), std::move(*opened)});
    return {};
}

std::expected<void, StorageError> StorageEngine::drop_collection(common::CollectionId id)
{
    if (!collections_.contains(id)) {
        return std::unexpected(make_error(StorageErrorCode::CollectionNotFound, "Collection not found"));
    }
    collections_.erase(id);
    if (filesystem_ != nullptr) {
        auto removed = filesystem_->remove(store_path(id));
        if (!removed) {
            return std::unexpected(StorageError {
                StorageErrorCode::StoreError,
                std::move(removed.error().message),
                StorageStoreErrorCode::FileSystemError,
            });
        }
    }
    return {};
}

bool StorageEngine::contains_collection(common::CollectionId id) const noexcept
{
    return collections_.contains(id);
}

std::expected<void, StorageError> StorageEngine::validate(
    const schema::CollectionSchema & schema,
    const common::RecordData & data
) const
{
    if (data.values.size() != schema.columns().size()) {
        return std::unexpected(make_error(StorageErrorCode::ValueCountMismatch, "Record value count does not match schema"));
    }
    for (std::size_t index = 0; index < data.values.size(); ++index) {
        const auto & value = data.values[index];
        const auto & column = schema.columns()[index];
        if (value.is_null()) {
            if (!column.nullable()) {
                return std::unexpected(make_error(
                    StorageErrorCode::NullConstraintViolation,
                    "Column cannot be null: " + column.column_name()
                ));
            }
            continue;
        }
        if (!value.matches_type(column.type())) {
            return std::unexpected(make_error(
                StorageErrorCode::TypeMismatch,
                "Value type does not match column: " + column.column_name()
            ));
        }
        if (column.type().id == common::LogicalTypeId::Varchar
            && column.type().parameter
            && std::get<std::string>(value.data()).size() > *column.type().parameter) {
            return std::unexpected(make_error(
                StorageErrorCode::ValueTooLarge,
                "VARCHAR value exceeds declared length: " + column.column_name()
            ));
        }
        if (column.type().id == common::LogicalTypeId::Vector
            && column.type().parameter
            && std::get<common::VectorValue>(value.data()).size() != *column.type().parameter) {
            return std::unexpected(make_error(
                StorageErrorCode::TypeMismatch,
                "VECTOR dimension mismatch: " + column.column_name()
            ));
        }
    }
    io::BufferByteWriter bytes;
    io::BinaryWriter writer {bytes};
    if (!writer.write_u64(1) || !writer.write_u32(static_cast<std::uint32_t>(data.values.size()))) {
        return std::unexpected(make_error(StorageErrorCode::RecordTooLarge, "Unable to encode record"));
    }
    for (const auto & value : data.values) {
        if (!writer.write_value(value)) {
            return std::unexpected(make_error(StorageErrorCode::RecordTooLarge, "Unable to encode record"));
        }
    }
    if (bytes.bytes().size() + 24 > StorageStore::PageSize) {
        return std::unexpected(make_error(StorageErrorCode::RecordTooLarge, "Encoded record does not fit in a data page"));
    }
    return {};
}

std::expected<common::Record, StorageError> StorageEngine::get(
    common::CollectionId collection_id,
    common::RecordId record_id
) const
{
    const auto it = collections_.find(collection_id);
    if (it == collections_.end()) {
        return std::unexpected(make_error(StorageErrorCode::CollectionNotFound, "Collection not found"));
    }
    auto result = it->second.store->get(record_id);
    if (!result) {
        return std::unexpected(from_storage_store_error(std::move(result.error())));
    }
    return std::move(*result);
}

std::expected<common::RecordId, StorageError> StorageEngine::insert(
    common::CollectionId collection_id,
    common::RecordData data
)
{
    auto it = collections_.find(collection_id);
    if (it == collections_.end()) {
        return std::unexpected(make_error(StorageErrorCode::CollectionNotFound, "Collection not found"));
    }
    if (auto valid = validate(it->second.schema, data); !valid) {
        return std::unexpected(std::move(valid.error()));
    }
    auto result = it->second.store->insert(std::move(data));
    if (!result) {
        return std::unexpected(from_storage_store_error(std::move(result.error())));
    }
    return *result;
}

std::expected<void, StorageError> StorageEngine::update(
    common::CollectionId collection_id,
    common::RecordId record_id,
    common::RecordData data
)
{
    auto it = collections_.find(collection_id);
    if (it == collections_.end()) {
        return std::unexpected(make_error(StorageErrorCode::CollectionNotFound, "Collection not found"));
    }
    if (auto valid = validate(it->second.schema, data); !valid) {
        return valid;
    }
    auto result = it->second.store->update(record_id, std::move(data));
    if (!result) {
        return std::unexpected(from_storage_store_error(std::move(result.error())));
    }
    return {};
}

std::expected<void, StorageError> StorageEngine::erase(
    common::CollectionId collection_id,
    common::RecordId record_id
)
{
    auto it = collections_.find(collection_id);
    if (it == collections_.end()) {
        return std::unexpected(make_error(StorageErrorCode::CollectionNotFound, "Collection not found"));
    }
    auto result = it->second.store->erase(record_id);
    if (!result) {
        return std::unexpected(from_storage_store_error(std::move(result.error())));
    }
    return {};
}

std::expected<StorageCursor, StorageError> StorageEngine::scan(common::CollectionId collection_id) const
{
    const auto it = collections_.find(collection_id);
    if (it == collections_.end()) {
        return std::unexpected(make_error(StorageErrorCode::CollectionNotFound, "Collection not found"));
    }
    return it->second.store->scan();
}

void StorageEngine::clear() noexcept
{
    collections_.clear();
}

} // namespace litedb::core::storage
