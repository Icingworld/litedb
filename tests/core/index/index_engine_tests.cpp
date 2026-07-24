#include "core/meta/meta_engine.hpp"
#include "core/index/index_engine.hpp"
#include "core/storage/schema_loader.hpp"
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
        throw std::runtime_error(result.error().message());
    }
    return std::move(*result);
}

std::vector<RecordId> ids(std::expected<std::vector<RecordId>, IndexError> result)
{
    if (!result.has_value()) {
        throw std::runtime_error(result.error().message());
    }
    return std::move(*result);
}

struct Fixture
{
    litedb::tests::TemporaryDirectory storage_directory {"litedb-index-engine-tests"};
    litedb::core::filesystem::FileSystem filesystem {litedb::core::filesystem::create_platform_filesystem()};
    CatalogEditor catalog;
    StorageEngine storage {
        storage_directory.path(),
        filesystem,
        litedb::core::storage::StorageOpenMode::TransactionalStaging,
    };
    DatabaseId database_id {0};
    CollectionId users_id {0};
    ColumnId age_column_id {0};

    Fixture()
    {
        auto database = catalog.create_database(CreateDatabaseRequest {.name = "demo"});
        if (!database.has_value()) {
            throw std::runtime_error(database.error().message());
        }
        database_id = *database;

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
            throw std::runtime_error(collection.error().message());
        }
        users_id = *collection;

        const auto * age_column = catalog.view().find_column(users_id, "age");
        require(age_column != nullptr, "age column missing");
        age_column_id = age_column->id();

        auto collection_schema = load_collection_schema(catalog.view(), users_id);
        if (!collection_schema.has_value()) {
            throw std::runtime_error(collection_schema.error().message);
        }
        auto created_storage = storage.create_collection(std::move(collection_schema.value()));
        if (!created_storage.has_value()) {
            throw std::runtime_error(created_storage.error().message());
        }
    }

    RecordId insert_user(std::int64_t id, std::optional<std::int32_t> age)
    {
        RecordData record_data;
        record_data.values.push_back(Value {id});
        record_data.values.push_back(age.has_value() ? Value {age.value()} : Value::null());
        auto inserted = storage.insert(users_id, std::move(record_data));
        if (!inserted.has_value()) {
            throw std::runtime_error(inserted.error().message());
        }
        return *inserted;
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
            throw std::runtime_error(std::string {created.error().message()});
        }
        const auto * index = catalog.view().find_index(*created);
        require(index != nullptr, "catalog index missing");
        return *index;
    }

    CollectionSchema users_schema() const
    {
        auto collection_schema = load_collection_schema(catalog.view(), users_id);
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

    IndexEngine engine {fixture.storage_directory.path(), fixture.filesystem};
    auto created = engine.create_index(index_entry, fixture.users_schema(), fixture.storage);
    require(created.has_value(), "create index failed");

    auto view = engine.find_index(index_entry.id());
    require(view.has_value(), "managed index missing");
    require(view->collection_id == fixture.users_id, "managed index collection mismatch");
    require(view->column_id == fixture.age_column_id, "managed index column mismatch");
    require(view->key_type.id == LogicalTypeId::Integer, "managed index key type mismatch");
    require(view->kind == litedb::core::index::IndexKind::BTree, "managed index kind mismatch");
    require(!view->unique, "managed index unique mismatch");
    require(view->entry_count == 2, "NULL value should not be indexed");
    require(ids(engine.find_equal(index_entry.id(), key(Value {std::int32_t {18}}))) == std::vector<RecordId> {1}, "index lookup mismatch");

    require(
        ids(engine.scan_range(
            index_entry.id(),
            IndexRange::closed(key(Value {std::int32_t {18}}), key(Value {std::int32_t {20}}))
        )) == std::vector<RecordId>({1, 3}),
        "managed range scan mismatch"
    );

    auto wrong_type = engine.find_equal(index_entry.id(), key(Value {std::int64_t {18}}));
    require(!wrong_type.has_value(), "lookup with mismatched physical key type should fail");
    require(wrong_type.error().is(IndexErrorCode::KeyTypeMismatch), "lookup key type error mismatch");
}

void test_insert_update_delete_maintenance()
{
    Fixture fixture;
    fixture.insert_user(1, 18);
    const auto & index_entry = fixture.create_catalog_index("idx_age", litedb::core::meta::entry::IndexKind::BTree);

    IndexEngine engine {fixture.storage_directory.path(), fixture.filesystem};
    auto created = engine.create_index(index_entry, fixture.users_schema(), fixture.storage);
    require(created.has_value(), "create index failed");

    RecordData null_age {.values = {Value {std::int64_t {2}}, Value::null()}};
    auto null_insert = engine.prepare_insert(fixture.users_id, null_age);
    require(null_insert.has_value(), "NULL insert prepare should succeed");
    require(null_insert->empty(), "NULL insert should not create index binding");

    RecordData age_20 {.values = {Value {std::int64_t {2}}, Value {std::int32_t {20}}}};
    auto insert = engine.prepare_insert(fixture.users_id, age_20);
    require(insert.has_value(), "insert prepare failed");
    require(insert->size() == 1, "insert binding count mismatch");
    require(engine.on_insert(2, *insert).has_value(), "on_insert failed");

    RecordData wrong_age_type {.values = {Value {std::int64_t {3}}, Value {std::int64_t {20}}}};
    auto wrong_insert = engine.prepare_insert(fixture.users_id, wrong_age_type);
    require(!wrong_insert.has_value(), "mismatched indexed value type should fail during prepare");
    require(wrong_insert.error().is(IndexErrorCode::KeyTypeMismatch), "prepare key type error mismatch");

    auto view = engine.find_index(index_entry.id());
    require(view.has_value(), "managed index missing");
    require(ids(engine.find_equal(index_entry.id(), key(Value {std::int32_t {20}}))) == std::vector<RecordId> {2}, "inserted index key mismatch");

    RecordData age_21 {.values = {Value {std::int64_t {2}}, Value {std::int32_t {21}}}};
    auto update = engine.prepare_update(fixture.users_id, age_20, age_21);
    require(update.has_value(), "update prepare failed");
    require(engine.on_update(2, *update).has_value(), "on_update failed");
    require(ids(engine.find_equal(index_entry.id(), key(Value {std::int32_t {20}}))).empty(), "old update key should be erased");
    require(ids(engine.find_equal(index_entry.id(), key(Value {std::int32_t {21}}))) == std::vector<RecordId> {2}, "new update key mismatch");

    auto del = engine.prepare_delete(fixture.users_id, age_21);
    require(del.has_value(), "delete prepare failed");
    require(engine.on_delete(2, *del).has_value(), "on_delete failed");
    require(ids(engine.find_equal(index_entry.id(), key(Value {std::int32_t {21}}))).empty(), "deleted index key should be erased");
}

void test_unique_index_rejects_duplicates()
{
    Fixture fixture;
    fixture.insert_user(1, 18);
    fixture.insert_user(2, 18);
    const auto & duplicate_index = fixture.create_catalog_index("idx_age_unique", litedb::core::meta::entry::IndexKind::BTree, true);

    IndexEngine engine {fixture.storage_directory.path(), fixture.filesystem};
    auto duplicate_build = engine.create_index(duplicate_index, fixture.users_schema(), fixture.storage);
    require(!duplicate_build.has_value(), "unique index build should reject duplicates");
    require(duplicate_build.error().is(IndexErrorCode::DuplicateKey), "unique duplicate build error mismatch");

    Fixture clean_fixture;
    clean_fixture.insert_user(1, 18);
    const auto & unique_index = clean_fixture.create_catalog_index("idx_age_unique", litedb::core::meta::entry::IndexKind::BTree, true);
    IndexEngine clean_engine {clean_fixture.storage_directory.path(), clean_fixture.filesystem};
    auto created = clean_engine.create_index(unique_index, clean_fixture.users_schema(), clean_fixture.storage);
    require(created.has_value(), "unique index create failed");

    RecordData duplicate {.values = {Value {std::int64_t {2}}, Value {std::int32_t {18}}}};
    auto duplicate_insert = clean_engine.prepare_insert(clean_fixture.users_id, duplicate);
    require(!duplicate_insert.has_value(), "unique index prepare insert should reject duplicate");
    require(duplicate_insert.error().is(IndexErrorCode::DuplicateKey), "unique duplicate insert error mismatch");
}

void test_restore_all_is_atomic_on_failure()
{
    Fixture fixture;
    fixture.insert_user(1, 18);
    const auto & index_entry = fixture.create_catalog_index("idx_age", litedb::core::meta::entry::IndexKind::BTree);

    IndexEngine engine {fixture.storage_directory.path(), fixture.filesystem};
    auto created = engine.create_index(index_entry, fixture.users_schema(), fixture.storage);
    require(created.has_value(), "initial create index failed");
    require(engine.find_index(index_entry.id()).has_value(), "initial index missing");

    const auto & missing_index = fixture.create_catalog_index("idx_age_missing", litedb::core::meta::entry::IndexKind::BTree);
    auto restored = engine.restore_all(fixture.catalog.view(), fixture.storage);
    require(!restored.has_value(), "restore should fail when a persistent index file is missing");
    require(restored.error().is(IndexErrorCode::StorageError), "restore storage error mismatch");
    require(restored.error().category() == litedb::core::error::ErrorCategory::Index,
            "restore error should retain the index category");
    const auto * context = restored.error().context<IndexErrorContext>();
    require(context != nullptr, "restore storage error should retain typed context");
    require(context->operation == IndexOperation::Open, "restore error operation mismatch");
    require(context->index_id == missing_index.id(), "restore error index id mismatch");
    require(context->path.filename() == std::to_string(missing_index.id()) + ".bti",
            "restore error path mismatch");
    require(context->source_code.has_value(), "restore error source code is missing");
    require(engine.find_index(index_entry.id()).has_value(), "failed restore should keep existing indexes");
    require(!engine.find_index(missing_index.id()).has_value(), "failed restore should not publish partial indexes");
}

} // namespace

int main()
{
    try {
        test_build_skips_nulls_and_views_index();
        test_insert_update_delete_maintenance();
        test_unique_index_rejects_duplicates();
        test_restore_all_is_atomic_on_failure();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
