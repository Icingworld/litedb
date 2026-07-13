#include "core/meta/meta_engine.hpp"
#include "core/index/index_manager.hpp"
#include "core/schema/schema_loader.hpp"
#include "core/storage/storage_engine.hpp"
#include "core/filesystem/platform_filesystem.hpp"
#include "../storage/temporary_directory.hpp"

#include <cstdint>
#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <vector>

namespace
{

using namespace litedb::core::meta;
using namespace litedb::core::meta::entry;
using namespace litedb::core::common;
using namespace litedb::core::index;
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

ScalarIndexKey key(Value value)
{
    auto result = ScalarIndexKey::from_value(std::move(value));
    if (!result.has_value()) {
        throw std::runtime_error(result.error().message);
    }
    return std::move(result.value());
}

std::vector<RecordId> ids(std::expected<std::vector<RecordId>, IndexError> result)
{
    if (!result.has_value()) {
        throw std::runtime_error(result.error().message);
    }
    return std::move(result.value());
}

struct Fixture
{
    litedb::tests::TemporaryDirectory storage_directory {"litedb-index-manager-tests"};
    litedb::core::filesystem::FileSystem filesystem {litedb::core::filesystem::create_platform_filesystem()};
    MetaEngine catalog;
    StorageEngine storage {storage_directory.path(), filesystem};
    DatabaseId database_id {0};
    CollectionId users_id {0};
    ColumnId age_column_id {0};

    Fixture()
    {
        auto database = catalog.create_database(CreateDatabaseRequest {.name = "demo"});
        if (!database.has_value()) {
            throw std::runtime_error(database.error().message);
        }
        database_id = database.value();

        auto collection = catalog.create_collection(CreateCollectionRequest {
            .database_id = database_id,
            .name = "users",
            .columns = {
                ColumnDefinition {
                    .name = "id",
                    .type = type(LogicalTypeId::BigInt),
                    },
                ColumnDefinition {
                    .name = "age",
                    .type = type(LogicalTypeId::Integer),
                    .nullable = true,
                },
            },
        });
        if (!collection.has_value()) {
            throw std::runtime_error(collection.error().message);
        }
        users_id = collection.value();

        const auto * age_column = catalog.find_column(users_id, "age");
        require(age_column != nullptr, "age column missing");
        age_column_id = age_column->id();

        auto collection_schema = load_collection_schema(catalog, users_id);
        if (!collection_schema.has_value()) {
            throw std::runtime_error(collection_schema.error().message);
        }
        auto created_storage = storage.create_collection(std::move(collection_schema.value()));
        if (!created_storage.has_value()) {
            throw std::runtime_error(created_storage.error().message);
        }
    }

    RecordId insert_user(std::int64_t id, std::optional<std::int32_t> age)
    {
        RecordData record_data;
        record_data.values.push_back(Value {id});
        record_data.values.push_back(age.has_value() ? Value {age.value()} : Value::null());
        auto inserted = storage.insert(users_id, std::move(record_data));
        if (!inserted.has_value()) {
            throw std::runtime_error(inserted.error().message);
        }
        return inserted.value();
    }

    const IndexEntry & create_catalog_index(std::string name, litedb::core::meta::entry::IndexKind kind, bool unique = false)
    {
        auto created = catalog.create_index(CreateIndexRequest {
            .collection_id = users_id,
            .column_ids = {age_column_id},
            .name = std::move(name),
            .kind = kind,
            .unique = unique,
        });
        if (!created.has_value()) {
            throw std::runtime_error(created.error().message);
        }
        const auto * index = catalog.find_index(created.value());
        require(index != nullptr, "catalog index missing");
        return *index;
    }

    CollectionSchema users_schema() const
    {
        auto collection_schema = load_collection_schema(catalog, users_id);
        if (!collection_schema.has_value()) {
            throw std::runtime_error(collection_schema.error().message);
        }
        return std::move(collection_schema.value());
    }
};

void test_build_skips_nulls_and_views_index()
{
    Fixture fixture;
    fixture.insert_user(1, 18);
    fixture.insert_user(2, std::nullopt);
    fixture.insert_user(3, 20);
    const auto & index_entry = fixture.create_catalog_index("idx_age", litedb::core::meta::entry::IndexKind::BTree);

    IndexManager manager;
    auto created = manager.create_index(index_entry, fixture.users_schema(), fixture.storage);
    require(created.has_value(), "create index failed");

    auto view = manager.find_index(index_entry.id());
    require(view.has_value(), "managed index missing");
    require(view->collection_id == fixture.users_id, "managed index collection mismatch");
    require(view->column_id == fixture.age_column_id, "managed index column mismatch");
    require(view->kind == litedb::core::index::IndexKind::BTree, "managed index kind mismatch");
    require(!view->unique, "managed index unique mismatch");
    require(view->index.size() == 2, "NULL value should not be indexed");
    require(ids(view->index.find_equal(key(Value {std::int32_t {18}}))) == std::vector<RecordId> {1}, "index lookup mismatch");
}

void test_insert_update_delete_maintenance()
{
    Fixture fixture;
    fixture.insert_user(1, 18);
    const auto & index_entry = fixture.create_catalog_index("idx_age", litedb::core::meta::entry::IndexKind::Hash);

    IndexManager manager;
    auto created = manager.create_index(index_entry, fixture.users_schema(), fixture.storage);
    require(created.has_value(), "create index failed");

    RecordData null_age {.values = {Value {std::int64_t {2}}, Value::null()}};
    auto null_insert = manager.prepare_insert(fixture.users_id, null_age);
    require(null_insert.has_value(), "NULL insert prepare should succeed");
    require(null_insert->empty(), "NULL insert should not create index binding");

    RecordData age_20 {.values = {Value {std::int64_t {2}}, Value {std::int32_t {20}}}};
    auto insert = manager.prepare_insert(fixture.users_id, age_20);
    require(insert.has_value(), "insert prepare failed");
    require(insert->size() == 1, "insert binding count mismatch");
    require(manager.on_insert(2, insert.value()).has_value(), "on_insert failed");

    auto view = manager.find_index(index_entry.id());
    require(view.has_value(), "managed index missing");
    require(ids(view->index.find_equal(key(Value {std::int32_t {20}}))) == std::vector<RecordId> {2}, "inserted index key mismatch");

    RecordData age_21 {.values = {Value {std::int64_t {2}}, Value {std::int32_t {21}}}};
    auto update = manager.prepare_update(fixture.users_id, age_20, age_21);
    require(update.has_value(), "update prepare failed");
    require(manager.on_update(2, update.value()).has_value(), "on_update failed");
    require(ids(view->index.find_equal(key(Value {std::int32_t {20}}))).empty(), "old update key should be erased");
    require(ids(view->index.find_equal(key(Value {std::int32_t {21}}))) == std::vector<RecordId> {2}, "new update key mismatch");

    auto del = manager.prepare_delete(fixture.users_id, age_21);
    require(del.has_value(), "delete prepare failed");
    require(manager.on_delete(2, del.value()).has_value(), "on_delete failed");
    require(ids(view->index.find_equal(key(Value {std::int32_t {21}}))).empty(), "deleted index key should be erased");
}

void test_unique_index_rejects_duplicates()
{
    Fixture fixture;
    fixture.insert_user(1, 18);
    fixture.insert_user(2, 18);
    const auto & duplicate_index = fixture.create_catalog_index("idx_age_unique", litedb::core::meta::entry::IndexKind::BTree, true);

    IndexManager manager;
    auto duplicate_build = manager.create_index(duplicate_index, fixture.users_schema(), fixture.storage);
    require(!duplicate_build.has_value(), "unique index build should reject duplicates");
    require(duplicate_build.error().code == IndexErrorCode::DuplicateKey, "unique duplicate build error mismatch");

    Fixture clean_fixture;
    clean_fixture.insert_user(1, 18);
    const auto & unique_index = clean_fixture.create_catalog_index("idx_age_unique", litedb::core::meta::entry::IndexKind::BTree, true);
    auto created = manager.create_index(unique_index, clean_fixture.users_schema(), clean_fixture.storage);
    require(created.has_value(), "unique index create failed");

    RecordData duplicate {.values = {Value {std::int64_t {2}}, Value {std::int32_t {18}}}};
    auto duplicate_insert = manager.prepare_insert(clean_fixture.users_id, duplicate);
    require(!duplicate_insert.has_value(), "unique index prepare insert should reject duplicate");
    require(duplicate_insert.error().code == IndexErrorCode::DuplicateKey, "unique duplicate insert error mismatch");
}

void test_rebuild_all_is_atomic_on_failure()
{
    Fixture fixture;
    fixture.insert_user(1, 18);
    const auto & index_entry = fixture.create_catalog_index("idx_age", litedb::core::meta::entry::IndexKind::BTree);

    IndexManager manager;
    auto created = manager.create_index(index_entry, fixture.users_schema(), fixture.storage);
    require(created.has_value(), "initial create index failed");
    require(manager.find_index(index_entry.id()).has_value(), "initial index missing");

    fixture.insert_user(2, 18);
    const auto & unique_index = fixture.create_catalog_index("idx_age_unique", litedb::core::meta::entry::IndexKind::Hash, true);
    auto rebuilt = manager.rebuild_all(fixture.catalog, fixture.storage);
    require(!rebuilt.has_value(), "rebuild should fail on duplicate unique key");
    require(rebuilt.error().code == IndexErrorCode::DuplicateKey, "rebuild duplicate error mismatch");
    require(manager.find_index(index_entry.id()).has_value(), "failed rebuild should keep existing indexes");
    require(!manager.find_index(unique_index.id()).has_value(), "failed rebuild should not publish partial indexes");
}

} // namespace

int main()
{
    try {
        test_build_skips_nulls_and_views_index();
        test_insert_update_delete_maintenance();
        test_unique_index_rejects_duplicates();
        test_rebuild_all_is_atomic_on_failure();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
