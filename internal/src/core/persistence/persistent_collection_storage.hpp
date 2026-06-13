#pragma once

#include <expected>
#include <memory>
#include <unordered_map>
#include <vector>

#include "core/persistence/row_log.hpp"
#include "core/schema/collection.hpp"
#include "core/storage/collection_storage.hpp"

namespace litedb::core::persistence
{

class PersistentCollectionStorage final : public storage::CollectionStorage
{
public:
    static std::expected<std::unique_ptr<PersistentCollectionStorage>, storage::StorageError> open(
        schema::CollectionSchema collection_schema,
        RowLog row_log
    );

    PersistentCollectionStorage(schema::CollectionSchema collection_schema, RowLog row_log);

public:
    [[nodiscard]]
    const schema::CollectionSchema & collection_schema() const noexcept;

    [[nodiscard]]
    std::expected<schema::Record, storage::StorageError> get(common::RecordId record_id) const override;

    std::expected<common::RecordId, storage::StorageError> insert(schema::RecordData record_data) override;

    std::expected<void, storage::StorageError> update(
        common::RecordId record_id,
        schema::RecordData record_data
    ) override;

    std::expected<void, storage::StorageError> erase(common::RecordId record_id) override;

    [[nodiscard]]
    std::unique_ptr<storage::RecordCursor> scan() const override;

private:
    [[nodiscard]]
    std::expected<void, storage::StorageError> replay();

    [[nodiscard]]
    std::expected<void, storage::StorageError> validate_record(const schema::RecordData & record_data) const;

    void apply_upsert(common::RecordId record_id, schema::RecordData record_data);

    void apply_delete(common::RecordId record_id);

private:
    schema::CollectionSchema collection_schema_;
    RowLog row_log_;
    common::RecordId next_record_id_ {1};
    std::vector<common::RecordId> record_ids_;
    std::unordered_map<common::RecordId, schema::RecordData> records_;
};

} // namespace litedb::core::persistence
