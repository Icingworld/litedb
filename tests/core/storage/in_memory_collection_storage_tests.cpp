#include "core/schema/collection.hpp"
#include "core/storage/in_memory_collection_storage.hpp"
#include "core/storage/storage_manager.hpp"

#include <exception>
#include <iostream>
#include <stdexcept>

namespace
{

using namespace litedb::core::common;
using namespace litedb::core::schema;
using namespace litedb::core::storage;

void require(bool condition, const char * message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

LogicalType type(LogicalTypeId id, std::optional<std::size_t> parameter = std::nullopt)
{
    return LogicalType {id, parameter};
}

CollectionSchema users_schema()
{
    std::vector<ColumnSchema> columns;
    columns.emplace_back(1, 1, 0, "id", type(LogicalTypeId::BigInt), false, true, true, std::nullopt, std::nullopt);
    columns.emplace_back(2, 1, 1, "name", type(LogicalTypeId::Varchar, 64), true, false, false, std::nullopt, std::nullopt);
    columns.emplace_back(3, 1, 2, "score", type(LogicalTypeId::Double), true, false, false, std::nullopt, std::nullopt);
    columns.emplace_back(4, 1, 3, "embedding", type(LogicalTypeId::Vector, 3), true, false, false, std::nullopt, std::nullopt);
    return CollectionSchema {1, 1, "users", std::move(columns)};
}

RecordData record(std::int64_t id, std::string name, double score)
{
    return RecordData {
        .values = {
            Value {id},
            Value {std::move(name)},
            Value {score},
            Value {VectorValue {0.1, 0.2, 0.3}},
        },
    };
}

void test_insert_scan_and_delete()
{
    InMemoryCollectionStorage storage(users_schema());

    auto first = storage.insert(record(1, "alice", 9.5));
    auto second = storage.insert(record(2, "bob", 8.0));
    require(first.has_value(), "first insert failed");
    require(second.has_value(), "second insert failed");
    require(first.value() == 1, "first record id mismatch");
    require(second.value() == 2, "second record id mismatch");

    auto cursor = storage.scan();
    auto first_record = cursor->next();
    auto second_record = cursor->next();
    auto end = cursor->next();

    require(first_record.has_value(), "first scanned record missing");
    require(second_record.has_value(), "second scanned record missing");
    require(!end.has_value(), "scan should be exhausted");
    require(first_record->record_id == first.value(), "first scanned id mismatch");
    require(second_record->record_id == second.value(), "second scanned id mismatch");

    auto erased = storage.erase(first.value());
    require(erased.has_value(), "erase existing record failed");

    cursor = storage.scan();
    auto remaining = cursor->next();
    require(remaining.has_value(), "remaining record missing");
    require(remaining->record_id == second.value(), "remaining record id mismatch");
    require(!cursor->next().has_value(), "scan should contain one remaining record");

    auto missing = storage.erase(999);
    require(!missing.has_value(), "erase missing record should fail");
    require(missing.error().code == StorageErrorCode::RecordNotFound, "missing record error mismatch");
}

void test_insert_validation()
{
    InMemoryCollectionStorage storage(users_schema());

    auto wrong_count = storage.insert(RecordData {.values = {Value {std::int64_t {1}}}});
    require(!wrong_count.has_value(), "wrong value count should fail");
    require(wrong_count.error().code == StorageErrorCode::ValueCountMismatch, "wrong count error mismatch");

    auto wrong_type = storage.insert(RecordData {
        .values = {
            Value {std::string {"not bigint"}},
            Value {std::string {"alice"}},
            Value {9.5},
            Value {VectorValue {0.1, 0.2, 0.3}},
        },
    });
    require(!wrong_type.has_value(), "wrong type should fail");
    require(wrong_type.error().code == StorageErrorCode::TypeMismatch, "wrong type error mismatch");

    auto null_primary_key = storage.insert(RecordData {
        .values = {
            Value::null(),
            Value {std::string {"alice"}},
            Value {9.5},
            Value {VectorValue {0.1, 0.2, 0.3}},
        },
    });
    require(!null_primary_key.has_value(), "null primary key should fail");
    require(null_primary_key.error().code == StorageErrorCode::NullConstraintViolation, "null error mismatch");

    auto nullable_column = storage.insert(RecordData {
        .values = {
            Value {std::int64_t {1}},
            Value::null(),
            Value {9.5},
            Value {VectorValue {0.1, 0.2, 0.3}},
        },
    });
    require(nullable_column.has_value(), "nullable column insert should succeed");
}

void test_storage_manager()
{
    StorageManager manager;
    auto created = manager.create_collection(users_schema());
    require(created.has_value(), "create collection storage failed");
    require(manager.find_collection(1) != nullptr, "collection storage lookup failed");

    auto duplicate = manager.create_collection(users_schema());
    require(!duplicate.has_value(), "duplicate collection storage should fail");
    require(duplicate.error().code == StorageErrorCode::CollectionAlreadyExists, "duplicate storage error mismatch");

    auto dropped = manager.drop_collection(1);
    require(dropped.has_value(), "drop collection storage failed");
    require(manager.find_collection(1) == nullptr, "dropped collection storage should be missing");

    auto missing = manager.drop_collection(1);
    require(!missing.has_value(), "drop missing collection storage should fail");
    require(missing.error().code == StorageErrorCode::CollectionNotFound, "missing collection storage error mismatch");
}

} // namespace

int main()
{
    try {
        test_insert_scan_and_delete();
        test_insert_validation();
        test_storage_manager();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
