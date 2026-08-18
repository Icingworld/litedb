#include "core/storage/storage_engine.hpp"

#include <string>
#include <utility>

#include "core/common/logical_type.hpp"

namespace litedb::core::storage
{

namespace
{

StorageError make_error(
    StorageErrorCode code,
    std::string message,
    StorageOperation operation,
    common::CollectionId collection_id = 0,
    common::RecordId record_id = 0,
    std::optional<std::uint16_t> source_code = {}
)
{
    return make_storage_error(code, std::move(message), {
        .operation = operation,
        .collection_id = collection_id,
        .record_id = record_id,
        .source_code = source_code,
    });
}

StorageError filesystem_error(
    error::Error source,
    StorageOperation operation,
    common::CollectionId collection_id
)
{
    return make_error(
        StorageErrorCode::FileSystemFailure,
        source.message(),
        operation,
        collection_id,
        0,
        source.encode_code()
    );
}

} // namespace

StorageEngine::StorageEngine(
    std::filesystem::path data_directory,
    filesystem::FileSystem & filesystem,
    StorageOpenMode mode
) noexcept
    : data_directory_(std::move(data_directory))
    , filesystem_(&filesystem)
    , mode_(mode)
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
    if (mode_ != StorageOpenMode::TransactionalStaging) {
        return std::unexpected(make_error(
            StorageErrorCode::InvalidState,
            "Live storage engine is read-only",
            StorageOperation::Create,
            id
        ));
    }
    if (collections_.contains(id)) {
        return std::unexpected(make_error(
            StorageErrorCode::CollectionAlreadyExists,
            "Collection already exists",
            StorageOperation::Create,
            id
        ));
    }
    if (filesystem_ == nullptr) {
        return std::unexpected(make_error(
            StorageErrorCode::InvalidState,
            "Storage engine is not configured",
            StorageOperation::Create,
            id
        ));
    }
    auto exists = filesystem_->exists(store_path(id));
    if (!exists) return std::unexpected(filesystem_error(std::move(exists.error()), StorageOperation::Create, id));
    if (*exists) {
        return std::unexpected(make_error(
            StorageErrorCode::CollectionStoreAlreadyExists,
            "Collection store already exists",
            StorageOperation::Create,
            id
        ));
    }
    auto created = StorageStore::create(store_path(id), *filesystem_, id);
    if (!created) return std::unexpected(std::move(created.error()));
    collections_.emplace(id, CollectionState {std::move(schema), std::move(*created)});
    return {};
}

std::expected<void, StorageError> StorageEngine::open_collection(schema::CollectionSchema schema)
{
    const auto id = schema.collection_id();
    if (collections_.contains(id)) {
        return std::unexpected(make_error(
            StorageErrorCode::CollectionAlreadyExists,
            "Collection already exists",
            StorageOperation::Open,
            id
        ));
    }
    if (filesystem_ == nullptr) {
        return std::unexpected(make_error(
            StorageErrorCode::InvalidState,
            "Storage engine is not configured",
            StorageOperation::Open,
            id
        ));
    }
    auto exists = filesystem_->exists(store_path(id));
    if (!exists) return std::unexpected(filesystem_error(std::move(exists.error()), StorageOperation::Open, id));
    if (!*exists) {
        return std::unexpected(make_error(
            StorageErrorCode::CollectionStoreNotFound,
            "Collection store not found",
            StorageOperation::Open,
            id
        ));
    }
    auto opened = StorageStore::open(store_path(id), *filesystem_, id);
    if (!opened) return std::unexpected(std::move(opened.error()));
    collections_.emplace(id, CollectionState {std::move(schema), std::move(*opened)});
    return {};
}

std::expected<void, StorageError> StorageEngine::reload_collection(schema::CollectionSchema schema)
{
    const auto id = schema.collection_id();
    if (filesystem_ == nullptr) {
        return std::unexpected(make_error(
            StorageErrorCode::InvalidState,
            "Storage engine is not configured",
            StorageOperation::Reload,
            id
        ));
    }
    auto opened = StorageStore::open(store_path(id), *filesystem_, id);
    if (!opened) return std::unexpected(std::move(opened.error()));
    collections_.insert_or_assign(id, CollectionState {std::move(schema), std::move(*opened)});
    return {};
}

std::expected<void, StorageError> StorageEngine::drop_collection(common::CollectionId id)
{
    if (mode_ != StorageOpenMode::TransactionalStaging) {
        return std::unexpected(make_error(
            StorageErrorCode::InvalidState,
            "Live storage engine is read-only",
            StorageOperation::Drop,
            id
        ));
    }
    const auto it = collections_.find(id);
    if (it == collections_.end()) {
        return std::unexpected(make_error(
            StorageErrorCode::CollectionNotFound,
            "Collection not found",
            StorageOperation::Drop,
            id
        ));
    }
    collections_.erase(it);
    auto removed = filesystem_->remove(store_path(id));
    if (!removed) return std::unexpected(filesystem_error(std::move(removed.error()), StorageOperation::Drop, id));
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
        return std::unexpected(make_error(
            StorageErrorCode::ValueCountMismatch,
            "Record value count does not match schema",
            StorageOperation::Validate,
            schema.collection_id()
        ));
    }
    for (std::size_t index = 0; index < data.values.size(); ++index) {
        const auto & value = data.values[index];
        const auto & column = schema.columns()[index];
        if (value.is_null()) {
            if (!column.nullable()) {
                return std::unexpected(make_error(
                    StorageErrorCode::NullConstraintViolation,
                    "Column cannot be null: " + column.column_name(),
                    StorageOperation::Validate,
                    schema.collection_id()
                ));
            }
            continue;
        }
        if (!value.matches_type(column.type())) {
            return std::unexpected(make_error(
                StorageErrorCode::TypeMismatch,
                "Value type does not match column: " + column.column_name(),
                StorageOperation::Validate,
                schema.collection_id()
            ));
        }
        if (column.type().id == common::LogicalTypeId::Varchar &&
            column.type().parameter &&
            std::get<std::string>(value.data()).size() > *column.type().parameter) {
            return std::unexpected(make_error(
                StorageErrorCode::ValueTooLarge,
                "VARCHAR value exceeds declared length: " + column.column_name(),
                StorageOperation::Validate,
                schema.collection_id()
            ));
        }
        if (column.type().id == common::LogicalTypeId::Vector &&
            column.type().parameter &&
            std::get<common::VectorValue>(value.data()).size() != *column.type().parameter) {
            return std::unexpected(make_error(
                StorageErrorCode::TypeMismatch,
                "VECTOR dimension mismatch: " + column.column_name(),
                StorageOperation::Validate,
                schema.collection_id()
            ));
        }
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
        return std::unexpected(make_error(
            StorageErrorCode::CollectionNotFound,
            "Collection not found",
            StorageOperation::ReadPage,
            collection_id,
            record_id
        ));
    }
    return it->second.store->get(record_id);
}

std::expected<common::RecordId, StorageError> StorageEngine::insert(
    common::CollectionId collection_id,
    common::RecordData data
)
{
    if (mode_ != StorageOpenMode::TransactionalStaging) {
        return std::unexpected(make_error(
            StorageErrorCode::InvalidState,
            "Live storage engine is read-only",
            StorageOperation::Insert,
            collection_id
        ));
    }
    auto it = collections_.find(collection_id);
    if (it == collections_.end()) {
        return std::unexpected(make_error(
            StorageErrorCode::CollectionNotFound,
            "Collection not found",
            StorageOperation::Insert,
            collection_id
        ));
    }
    if (auto valid = validate(it->second.schema, data); !valid) return std::unexpected(std::move(valid.error()));
    return it->second.store->insert(std::move(data));
}

std::expected<void, StorageError> StorageEngine::update(
    common::CollectionId collection_id,
    common::RecordId record_id,
    common::RecordData data
)
{
    if (mode_ != StorageOpenMode::TransactionalStaging) {
        return std::unexpected(make_error(
            StorageErrorCode::InvalidState,
            "Live storage engine is read-only",
            StorageOperation::Update,
            collection_id,
            record_id
        ));
    }
    auto it = collections_.find(collection_id);
    if (it == collections_.end()) {
        return std::unexpected(make_error(
            StorageErrorCode::CollectionNotFound,
            "Collection not found",
            StorageOperation::Update,
            collection_id,
            record_id
        ));
    }
    if (auto valid = validate(it->second.schema, data); !valid) return std::unexpected(std::move(valid.error()));
    return it->second.store->update(record_id, std::move(data));
}

std::expected<void, StorageError> StorageEngine::erase(
    common::CollectionId collection_id,
    common::RecordId record_id
)
{
    if (mode_ != StorageOpenMode::TransactionalStaging) {
        return std::unexpected(make_error(
            StorageErrorCode::InvalidState,
            "Live storage engine is read-only",
            StorageOperation::Erase,
            collection_id,
            record_id
        ));
    }
    auto it = collections_.find(collection_id);
    if (it == collections_.end()) {
        return std::unexpected(make_error(
            StorageErrorCode::CollectionNotFound,
            "Collection not found",
            StorageOperation::Erase,
            collection_id,
            record_id
        ));
    }
    return it->second.store->erase(record_id);
}

std::expected<StorageCursor, StorageError> StorageEngine::scan(common::CollectionId collection_id) const
{
    const auto it = collections_.find(collection_id);
    if (it == collections_.end()) {
        return std::unexpected(make_error(
            StorageErrorCode::CollectionNotFound,
            "Collection not found",
            StorageOperation::Scan,
            collection_id
        ));
    }
    return it->second.store->scan();
}

void StorageEngine::clear() noexcept
{
    collections_.clear();
}

} // namespace litedb::core::storage
