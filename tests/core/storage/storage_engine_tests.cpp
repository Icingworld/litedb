#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#include "core/common/logical_type.hpp"
#include "core/filesystem/platform_filesystem.hpp"
#include "core/schema/collection.hpp"
#include "core/storage/storage_engine.hpp"

using namespace litedb::core;

namespace
{
void require(bool value, const char * message) { if (!value) throw std::runtime_error(message); }

const storage::StorageErrorContext & require_storage_error(
    const error::Error & actual,
    storage::StorageErrorCode code,
    storage::StorageOperation operation,
    const char * message
)
{
    require(actual.is(code), message);
    const auto * context = actual.context<storage::StorageErrorContext>();
    require(context != nullptr && context->operation == operation, message);
    return *context;
}

schema::CollectionSchema users_schema()
{
    return {1, 10, "users",
            {schema::ColumnSchema {1, 10, 0, "name", {common::LogicalTypeId::Varchar, 5000}, false, false, {}, {}},
             schema::ColumnSchema {2, 10, 1, "age", {common::LogicalTypeId::Integer, {}}, true, false, {}, {}}}};
}

common::RecordData user(std::string name, std::int32_t age)
{
    return {{common::Value {std::move(name)}, common::Value {age}}};
}

void contract(storage::StorageEngine & engine, bool open_existing)
{
    auto ready = open_existing ? engine.open_collection(users_schema()) : engine.create_collection(users_schema());
    require(ready.has_value(), "collection initialization failed");
    auto duplicate_create = engine.create_collection(users_schema());
    require(!duplicate_create, "duplicate collection create succeeded");
    require_storage_error(
        duplicate_create.error(),
        storage::StorageErrorCode::CollectionAlreadyExists,
        storage::StorageOperation::Create,
        "duplicate collection create operation mismatch"
    );
    auto duplicate_open = engine.open_collection(users_schema());
    require(!duplicate_open, "duplicate collection open succeeded");
    require_storage_error(
        duplicate_open.error(),
        storage::StorageErrorCode::CollectionAlreadyExists,
        storage::StorageOperation::Open,
        "duplicate collection open operation mismatch"
    );
    auto first = engine.insert(10, user("alice", 30));
    auto second = engine.insert(10, user("bob", 40));
    require(first && second && *first == 1 && *second == 2, "record ids must be monotonic");
    require(engine.get(10, *first)->data.values.size() == 2, "get failed");
    require(engine.update(10, *first, user(std::string(3000, 'x'), 31)).has_value(), "large update failed");
    require(engine.erase(10, *second).has_value(), "erase failed");
    auto cursor = engine.scan(10);
    require(cursor.has_value(), "scan failed");
    auto row = cursor->next();
    require(row && row->has_value() && (**row).id == *first, "cursor row mismatch");
    auto end = cursor->next();
    require(end && !end->has_value(), "cursor eof mismatch");
    auto invalid = engine.insert(10, {{common::Value {std::int32_t {1}}}});
    require(!invalid, "invalid record was inserted");
    require_storage_error(
        invalid.error(),
        storage::StorageErrorCode::ValueCountMismatch,
        storage::StorageOperation::Validate,
        "validation operation mismatch"
    );
    auto oversized = engine.insert(10, user(std::string(5000, 'z'), 1));
    require(!oversized, "oversized record accepted");
    require_storage_error(
        oversized.error(),
        storage::StorageErrorCode::RecordTooLarge,
        storage::StorageOperation::Encode,
        "oversized record operation mismatch"
    );

    auto missing_collection = engine.get(999, 1);
    require(!missing_collection, "missing collection get succeeded");
    const auto & get_context = require_storage_error(
        missing_collection.error(),
        storage::StorageErrorCode::CollectionNotFound,
        storage::StorageOperation::Get,
        "missing collection get operation mismatch"
    );
    require(get_context.collection_id == 999 && get_context.record_id == 1,
            "missing collection get context mismatch");

    auto missing_scan = engine.scan(999);
    require(!missing_scan, "missing collection scan succeeded");
    require_storage_error(
        missing_scan.error(),
        storage::StorageErrorCode::CollectionNotFound,
        storage::StorageOperation::Scan,
        "missing collection scan operation mismatch"
    );
}

void test_file_reopen()
{
    auto filesystem = filesystem::create_platform_filesystem();
    const auto path = std::filesystem::temp_directory_path() /
        ("litedb-storage-engine-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    {
        storage::StorageEngine engine {path, filesystem, storage::StorageOpenMode::TransactionalStaging};
        auto missing = engine.open_collection(users_schema());
        require(!missing, "missing collection store was opened");
        require_storage_error(
            missing.error(),
            storage::StorageErrorCode::CollectionStoreNotFound,
            storage::StorageOperation::Open,
            "missing collection store operation mismatch"
        );
    }
    {
        storage::StorageEngine engine {path, filesystem, storage::StorageOpenMode::TransactionalStaging};
        contract(engine, false);
        auto snapshot = engine.scan(10);
        require(snapshot.has_value(), "snapshot cursor creation failed");
        engine.clear();
        auto retained = snapshot->next();
        require(retained && retained->has_value() && (**retained).id == 1,
                "cursor did not retain records after engine clear");
    }
    {
        storage::StorageEngine engine {path, filesystem, storage::StorageOpenMode::TransactionalStaging};
        require(engine.open_collection(users_schema()).has_value(), "reopen failed");
        auto record = engine.get(10, 1);
        require(record && std::get<std::string>(record->data.values[0].data()).size() == 3000, "reopened record mismatch");
        auto next = engine.insert(10, user("carol", 50));
        require(next && *next == 3, "next record id was not persisted");
        {
            storage::StorageEngine live {path, filesystem};
            require(live.open_collection(users_schema()).has_value(), "live collection open failed");
            auto insert = live.insert(10, user("blocked", 1));
            auto update = live.update(10, 1, user("blocked", 1));
            auto erase = live.erase(10, 1);
            auto drop = live.drop_collection(10);
            require(!insert && !update && !erase && !drop, "live write bypassed transaction boundary");
            require_storage_error(
                insert.error(), storage::StorageErrorCode::InvalidState,
                storage::StorageOperation::Insert, "live insert operation mismatch"
            );
            require_storage_error(
                update.error(), storage::StorageErrorCode::InvalidState,
                storage::StorageOperation::Update, "live update operation mismatch"
            );
            require_storage_error(
                erase.error(), storage::StorageErrorCode::InvalidState,
                storage::StorageOperation::Erase, "live erase operation mismatch"
            );
            require_storage_error(
                drop.error(), storage::StorageErrorCode::InvalidState,
                storage::StorageOperation::Drop, "live drop operation mismatch"
            );
        }
        require(engine.drop_collection(10).has_value(), "drop failed");
        auto dropped_again = engine.drop_collection(10);
        require(!dropped_again, "missing collection drop succeeded");
        require_storage_error(
            dropped_again.error(),
            storage::StorageErrorCode::CollectionNotFound,
            storage::StorageOperation::Drop,
            "missing collection drop operation mismatch"
        );
    }
    std::filesystem::remove_all(path);
}
}

int main()
{
    try { test_file_reopen(); }
    catch (const std::exception & error) { std::cerr << error.what() << '\n'; return 1; }
    return 0;
}
