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
    auto first = engine.insert(10, user("alice", 30));
    auto second = engine.insert(10, user("bob", 40));
    require(first && second && *first == 1 && *second == 2, "record ids must be monotonic");
    require(engine.get(10, *first)->data.values.size() == 2, "get failed");
    require(engine.update(10, *first, user(std::string(3000, 'x'), 31)).has_value(), "large update failed");
    require(engine.erase(10, *second).has_value(), "erase failed");
    auto cursor = engine.scan(10);
    require(cursor.has_value(), "scan failed");
    auto row = cursor->next();
    require(row && row->has_value() && (**row).record_id == *first, "cursor row mismatch");
    auto end = cursor->next();
    require(end && !end->has_value(), "cursor eof mismatch");
    auto invalid = engine.insert(10, {{common::Value {std::int32_t {1}}}});
    require(!invalid && invalid.error().code == storage::StorageErrorCode::ValueCountMismatch, "validation mismatch");
    auto oversized = engine.insert(10, user(std::string(5000, 'z'), 1));
    require(!oversized && oversized.error().code == storage::StorageErrorCode::RecordTooLarge, "oversized record accepted");
}

void test_file_reopen()
{
    auto filesystem = filesystem::create_platform_filesystem();
    const auto path = std::filesystem::temp_directory_path() /
        ("litedb-storage-v2-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    {
        storage::StorageEngine engine {path, filesystem};
        contract(engine, false);
    }
    {
        storage::StorageEngine engine {path, filesystem};
        require(engine.open_collection(users_schema()).has_value(), "reopen failed");
        auto record = engine.get(10, 1);
        require(record && std::get<std::string>(record->data.values[0].data()).size() == 3000, "reopened record mismatch");
        auto next = engine.insert(10, user("carol", 50));
        require(next && *next == 3, "next record id was not persisted");
        require(engine.drop_collection(10).has_value(), "drop failed");
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
