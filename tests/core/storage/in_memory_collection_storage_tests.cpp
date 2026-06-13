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

void test_update()
{
    InMemoryCollectionStorage storage(users_schema());

    auto inserted = storage.insert(record(1, "alice", 9.5));
    require(inserted.has_value(), "insert before update failed");

    auto updated = storage.update(inserted.value(), record(1, "alice-updated", 10.0));
    require(updated.has_value(), "update existing record failed");

    auto cursor = storage.scan();
    auto scanned = cursor->next();
    require(scanned.has_value(), "updated record missing");
    require(scanned->record_id == inserted.value(), "update should keep record id stable");
    require(std::get<std::string>(scanned->data.values[1].data()) == "alice-updated", "updated name mismatch");
    require(std::get<double>(scanned->data.values[2].data()) == 10.0, "updated score mismatch");
    require(!cursor->next().has_value(), "scan should contain one updated record");

    auto invalid = storage.update(inserted.value(), RecordData {.values = {Value {std::int64_t {1}}}});
    require(!invalid.has_value(), "invalid update should fail");
    require(invalid.error().code == StorageErrorCode::ValueCountMismatch, "invalid update error mismatch");

    cursor = storage.scan();
    scanned = cursor->next();
    require(scanned.has_value(), "record should remain after failed update");
    require(std::get<std::string>(scanned->data.values[1].data()) == "alice-updated", "failed update should not modify record");

    auto missing = storage.update(999, record(999, "missing", 0.0));
    require(!missing.has_value(), "update missing record should fail");
    require(missing.error().code == StorageErrorCode::RecordNotFound, "missing update error mismatch");
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

void test_get()
{
    InMemoryCollectionStorage storage(users_schema());

    auto first = storage.insert(record(1, "alice", 9.5));
    auto second = storage.insert(record(2, "bob", 8.0));
    require(first.has_value(), "first insert failed");
    require(second.has_value(), "second insert failed");

    const InMemoryCollectionStorage & const_storage = storage;
    auto fetched = const_storage.get(first.value());
    require(fetched.has_value(), "get existing record failed");
    require(fetched->record_id == first.value(), "get record id mismatch");
    require(std::get<std::int64_t>(fetched->data.values[0].data()) == 1, "get id column mismatch");
    require(std::get<std::string>(fetched->data.values[1].data()) == "alice", "get name mismatch");
    require(std::get<double>(fetched->data.values[2].data()) == 9.5, "get score mismatch");

    auto fetched_second = const_storage.get(second.value());
    require(fetched_second.has_value(), "get second record failed");
    require(std::get<std::string>(fetched_second->data.values[1].data()) == "bob", "get second name mismatch");

    auto missing = const_storage.get(999);
    require(!missing.has_value(), "get missing record should fail");
    require(missing.error().code == StorageErrorCode::RecordNotFound, "get missing error mismatch");

    auto updated = storage.update(first.value(), record(1, "alice-updated", 10.0));
    require(updated.has_value(), "update before get failed");
    auto after_update = const_storage.get(first.value());
    require(after_update.has_value(), "get after update failed");
    require(std::get<std::string>(after_update->data.values[1].data()) == "alice-updated", "get after update name mismatch");
    require(std::get<double>(after_update->data.values[2].data()) == 10.0, "get after update score mismatch");

    auto erased = storage.erase(first.value());
    require(erased.has_value(), "erase before get failed");
    auto after_erase = const_storage.get(first.value());
    require(!after_erase.has_value(), "get erased record should fail");
    require(after_erase.error().code == StorageErrorCode::RecordNotFound, "get after erase error mismatch");

    auto copy_test = storage.insert(record(3, "carol", 7.5));
    require(copy_test.has_value(), "insert for copy test failed");
    auto copied = const_storage.get(copy_test.value());
    require(copied.has_value(), "get for copy test failed");
    auto mutating_data = copied->data;
    mutating_data.values[1] = Value {std::string {"mutated"}};
    auto unchanged = const_storage.get(copy_test.value());
    require(unchanged.has_value(), "get after local mutation failed");
    require(std::get<std::string>(unchanged->data.values[1].data()) == "carol", "get should return independent copy");
}

void test_get_via_storage_manager()
{
    StorageManager manager;
    auto created = manager.create_collection(users_schema());
    require(created.has_value(), "create collection for get test failed");

    auto * mutable_storage = manager.find_collection(1);
    require(mutable_storage != nullptr, "collection storage lookup failed");

    auto inserted = mutable_storage->insert(record(10, "manager", 5.5));
    require(inserted.has_value(), "insert via manager failed");

    const CollectionStorage * const_storage = manager.find_collection(1);
    require(const_storage != nullptr, "const collection storage lookup failed");

    auto fetched = const_storage->get(inserted.value());
    require(fetched.has_value(), "get via manager failed");
    require(fetched->record_id == inserted.value(), "get via manager id mismatch");
    require(std::get<std::int64_t>(fetched->data.values[0].data()) == 10, "get via manager column mismatch");
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
        test_update();
        test_get();
        test_get_via_storage_manager();
        test_insert_validation();
        test_storage_manager();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
