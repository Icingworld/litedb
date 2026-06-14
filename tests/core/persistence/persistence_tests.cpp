#include "core/engine/database_instance.hpp"
#include "core/engine/session.hpp"
#include "core/index/scalar_index_key.hpp"
#include "core/persistence/binary_io.hpp"
#include "core/persistence/catalog_store.hpp"
#include "core/persistence/manifest_store.hpp"
#include "core/persistence/persistent_collection_storage.hpp"
#include "core/persistence/row_log.hpp"
#include "core/storage/storage_error.hpp"

#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace
{

using namespace litedb::core;

void require(bool condition, const char * message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <typename T>
const T & get_value(const schema::Value & value)
{
    return std::get<T>(value.data());
}

std::vector<common::RecordId> find_index_equal(
    engine::DatabaseInstance & instance,
    common::IndexId index_id,
    schema::Value value
)
{
    auto key = index::ScalarIndexKey::from_value(std::move(value));
    require(key.has_value(), "index key creation failed");

    auto index_view = instance.index_manager().find_index(index_id);
    require(index_view.has_value(), "managed index missing");

    auto found = index_view->index.find_equal(key.value());
    require(found.has_value(), "index lookup failed");
    return std::move(found.value());
}

std::filesystem::path make_temp_dir(std::string name)
{
    auto path = std::filesystem::temp_directory_path() / std::move(name);
    std::filesystem::remove_all(path);
    std::filesystem::create_directories(path);
    return path;
}

executor::ExecutionResult execute_ok(engine::Session & session, std::string_view sql)
{
    auto result = session.execute_sql(sql);
    if (!result.has_value()) {
        throw std::runtime_error(result.error().message);
    }
    return std::move(result.value());
}

common::LogicalType type(common::LogicalTypeId id, std::optional<std::size_t> parameter = std::nullopt)
{
    return common::LogicalType {.id = id, .parameter = parameter};
}

void test_binary_value_roundtrip()
{
    std::stringstream stream {std::ios::in | std::ios::out | std::ios::binary};
    persistence::BinaryWriter writer {stream};
    writer.write_u32(42);
    writer.write_string("hello");
    writer.write_value(schema::Value {std::int64_t {7}});
    writer.write_value(schema::Value {schema::VectorValue {1.0, 2.0, 3.0}});

    stream.seekg(0);
    persistence::BinaryReader reader {stream};
    require(reader.read_u32() == 42, "u32 roundtrip mismatch");
    require(reader.read_string() == "hello", "string roundtrip mismatch");
    require(get_value<std::int64_t>(reader.read_value()) == 7, "bigint value mismatch");
    const auto vector = get_value<schema::VectorValue>(reader.read_value());
    require(vector.size() == 3, "vector size mismatch");
    require(vector[1] == 2.0, "vector value mismatch");
}

void test_manifest_and_catalog_store()
{
    const auto dir = make_temp_dir("litedb_manifest_catalog_test");
    persistence::ManifestStore manifest {dir};
    auto initialized = manifest.ensure_initialized();
    require(initialized.has_value(), "manifest init failed");
    require(std::filesystem::exists(dir / "manifest.ldb"), "manifest file missing");
    require(std::filesystem::exists(dir / "collections"), "collections dir missing");

    catalog::CatalogSnapshot snapshot;
    snapshot.next_database_id = 2;
    snapshot.next_collection_id = 2;
    snapshot.next_column_id = 3;
    snapshot.next_index_id = 2;
    snapshot.next_vector_index_id = 2;
    snapshot.databases.push_back(catalog::CatalogSnapshotDatabase {
        .id = 1,
        .name = "demo",
        .collections = {
            catalog::CatalogSnapshotCollection {
                .id = 1,
                .database_id = 1,
                .name = "users",
                .columns = {
                    catalog::CatalogSnapshotColumn {
                        .id = 1,
                        .name = "id",
                        .type = type(common::LogicalTypeId::BigInt),
                        .primary_key = true,
                        .unique = false,
                        .nullable = false,
                        .default_expression = std::nullopt,
                        .comment = std::string {"primary id"},
                    },
                    catalog::CatalogSnapshotColumn {
                        .id = 2,
                        .name = "embedding",
                        .type = type(common::LogicalTypeId::Vector, 3),
                        .primary_key = false,
                        .unique = false,
                        .nullable = true,
                        .default_expression = std::nullopt,
                        .comment = std::nullopt,
                    },
                },
                .indexes = {
                    catalog::CatalogSnapshotIndex {
                        .id = 1,
                        .column_id = 1,
                        .name = "idx_id",
                        .index_kind = catalog::CatalogIndexKind::BTree,
                        .unique = false,
                    },
                },
                .vector_indexes = {
                    catalog::CatalogSnapshotVectorIndex {
                        .id = 1,
                        .column_id = 2,
                        .name = "vidx_embedding",
                        .index_kind = catalog::CatalogVectorIndexKind::Hnsw,
                        .metric = catalog::CatalogVectorDistanceMetric::Cosine,
                        .dimension = 3,
                        .max_neighbors = 24,
                        .ef_construction = 240,
                        .ef_search_default = 80,
                        .random_seed = 7,
                    },
                },
            },
        },
    });

    persistence::CatalogStore store {manifest.catalog_path()};
    auto saved = store.save(snapshot);
    require(saved.has_value(), "catalog save failed");
    auto loaded = store.load_or_empty();
    require(loaded.has_value(), "catalog load failed");
    require(loaded->databases.size() == 1, "catalog database count mismatch");
    require(loaded->databases[0].collections[0].columns.size() == 2, "catalog column count mismatch");
    require(loaded->databases[0].collections[0].columns[1].type.parameter == 3, "catalog vector parameter mismatch");
    require(loaded->databases[0].collections[0].indexes.size() == 1, "catalog index count mismatch");
    require(loaded->databases[0].collections[0].indexes[0].name == "idx_id", "catalog index name mismatch");
    require(loaded->databases[0].collections[0].vector_indexes.size() == 1, "catalog vector index count mismatch");
    require(loaded->databases[0].collections[0].vector_indexes[0].name == "vidx_embedding", "catalog vector index name mismatch");
    require(loaded->databases[0].collections[0].vector_indexes[0].metric == catalog::CatalogVectorDistanceMetric::Cosine, "catalog vector index metric mismatch");
    require(loaded->databases[0].collections[0].vector_indexes[0].dimension == 3, "catalog vector index dimension mismatch");
}

schema::CollectionSchema simple_users_schema()
{
    std::vector<schema::ColumnSchema> columns;
    columns.emplace_back(1, 1, 0, "id", type(common::LogicalTypeId::BigInt), false, true, true, std::nullopt, std::nullopt);
    columns.emplace_back(2, 1, 1, "name", type(common::LogicalTypeId::Varchar, 64), true, false, false, std::nullopt, std::nullopt);
    return schema::CollectionSchema {1, 1, "users", std::move(columns)};
}

schema::RecordData simple_record(std::int64_t id, std::string name)
{
    return schema::RecordData {
        .values = {
            schema::Value {id},
            schema::Value {std::move(name)},
        },
    };
}

void test_persistent_collection_storage_get()
{
    const auto dir = make_temp_dir("litedb_persistent_get_test");
    persistence::RowLog log {dir / "1.rows", 1};
    auto opened = persistence::PersistentCollectionStorage::open(simple_users_schema(), std::move(log));
    require(opened.has_value(), "persistent collection storage open failed");

    auto & storage = **opened;
    auto inserted = storage.insert(simple_record(1, "alice"));
    require(inserted.has_value(), "persistent insert failed");

    const storage::CollectionStorage & const_storage = storage;
    auto fetched = const_storage.get(inserted.value());
    require(fetched.has_value(), "persistent get existing record failed");
    require(fetched->record_id == inserted.value(), "persistent get record id mismatch");
    require(get_value<std::int64_t>(fetched->data.values[0]) == 1, "persistent get id mismatch");
    require(get_value<std::string>(fetched->data.values[1]) == "alice", "persistent get name mismatch");

    auto missing = const_storage.get(999);
    require(!missing.has_value(), "persistent get missing record should fail");
    require(missing.error().code == storage::StorageErrorCode::RecordNotFound, "persistent get missing error mismatch");

    auto updated = storage.update(inserted.value(), simple_record(1, "alice-updated"));
    require(updated.has_value(), "persistent update before get failed");
    auto after_update = const_storage.get(inserted.value());
    require(after_update.has_value(), "persistent get after update failed");
    require(get_value<std::string>(after_update->data.values[1]) == "alice-updated", "persistent get after update mismatch");

    auto erased = storage.erase(inserted.value());
    require(erased.has_value(), "persistent erase before get failed");
    auto after_erase = const_storage.get(inserted.value());
    require(!after_erase.has_value(), "persistent get erased record should fail");
    require(after_erase.error().code == storage::StorageErrorCode::RecordNotFound, "persistent get after erase error mismatch");
}

void test_row_log_replay_and_partial_tail()
{
    const auto dir = make_temp_dir("litedb_row_log_test");
    const auto path = dir / "1.rows";
    persistence::RowLog log {path, 1};
    auto replay = log.replay_or_create();
    require(replay.has_value(), "row log create failed");

    schema::RecordData first {.values = {schema::Value {std::int64_t {1}}, schema::Value {"alice"}}};
    auto inserted = log.append_insert(1, first);
    require(inserted.has_value(), "row log insert failed");
    schema::RecordData updated {.values = {schema::Value {std::int64_t {1}}, schema::Value {"alice-updated"}}};
    auto update = log.append_update(1, updated);
    require(update.has_value(), "row log update failed");

    {
        std::ofstream out {path, std::ios::binary | std::ios::app};
        out.put('\x52');
        out.put('\x52');
    }

    auto replayed = log.replay_or_create();
    require(replayed.has_value(), "row log replay with partial tail failed");
    require(replayed->records.size() == 2, "row log replay count mismatch");
    require(replayed->next_record_id == 2, "row log next id mismatch");
}

void test_database_instance_reopens_persistent_data()
{
    const auto dir = make_temp_dir("litedb_reopen_test");

    {
        engine::DatabaseInstance instance {engine::DatabaseConfig {.data_dir = dir}};
        engine::Session session {instance};
        execute_ok(session, "CREATE DATABASE demo;");
        execute_ok(session, "USE demo;");
        execute_ok(
            session,
            "CREATE COLLECTION users ("
            "id BIGINT PRIMARY KEY, "
            "name VARCHAR(64) DEFAULT 'unknown', "
            "age INTEGER, "
            "embedding VECTOR(3)"
            ");"
        );
        execute_ok(session, "INSERT INTO users VALUES (1, 'alice', 18, [0.1, 0.2, 0.3]);");
        execute_ok(session, "INSERT INTO users VALUES (2, 'bob', 20, [0.2, 0.3, 0.4]);");
        execute_ok(session, "UPDATE users SET age = age + 1 WHERE id = 1;");
        execute_ok(session, "DELETE FROM users WHERE id = 2;");
    }

    {
        engine::DatabaseInstance reopened {engine::DatabaseConfig {.data_dir = dir}};
        engine::Session session {reopened};
        execute_ok(session, "USE demo;");
        auto selected = execute_ok(session, "SELECT name, age, embedding FROM users WHERE id = 1;");
        require(selected.kind == executor::ExecutionResultKind::RowSet, "reopen select result kind mismatch");
        require(selected.rows.size() == 1, "reopen row count mismatch");
        require(get_value<std::string>(selected.rows[0].values[0]) == "alice", "reopen name mismatch");
        require(get_value<std::int32_t>(selected.rows[0].values[1]) == 19, "reopen updated age mismatch");
        require(get_value<schema::VectorValue>(selected.rows[0].values[2]).size() == 3, "reopen vector mismatch");

        auto remaining = execute_ok(session, "SELECT id FROM users ORDER BY id ASC;");
        require(remaining.rows.size() == 1, "deleted row should not reappear");
    }
}

void test_index_ddl_reopen()
{
    const auto dir = make_temp_dir("litedb_index_ddl_reopen_test");

    {
        engine::DatabaseInstance instance {engine::DatabaseConfig {.data_dir = dir}};
        engine::Session session {instance};
        execute_ok(session, "CREATE DATABASE demo;");
        execute_ok(session, "USE demo;");
        execute_ok(session, "CREATE COLLECTION users (id BIGINT PRIMARY KEY, age INTEGER);");
        execute_ok(session, "INSERT INTO users VALUES (1, 18);");
        auto created = execute_ok(session, "CREATE INDEX idx_age ON users (age) USING HASH;");
        require(created.affected_rows == 1, "CREATE INDEX affected rows mismatch");

        const auto * database = instance.catalog().find_database("demo");
        require(database != nullptr, "created database lookup failed");
        const auto * collection = instance.catalog().find_collection(database->id(), "users");
        require(collection != nullptr, "created collection lookup failed");
        const auto * index = instance.catalog().find_index(collection->id(), "idx_age");
        require(index != nullptr, "created index lookup failed");
        require(index->index_kind() == catalog::CatalogIndexKind::Hash, "created index kind mismatch");
        require(find_index_equal(instance, index->id(), schema::Value {std::int32_t {18}}).size() == 1, "created index should include existing row");
    }

    common::CollectionId users_id {0};
    {
        engine::DatabaseInstance reopened {engine::DatabaseConfig {.data_dir = dir}};
        const auto * database = reopened.catalog().find_database("demo");
        require(database != nullptr, "reopened database missing");
        const auto * collection = reopened.catalog().find_collection(database->id(), "users");
        require(collection != nullptr, "reopened collection missing");
        users_id = collection->id();
        const auto * index = reopened.catalog().find_index(users_id, "idx_age");
        require(index != nullptr, "reopened index missing");
        require(index->index_kind() == catalog::CatalogIndexKind::Hash, "reopened index kind mismatch");
        const auto index_id = index->id();
        require(find_index_equal(reopened, index_id, schema::Value {std::int32_t {18}}).size() == 1, "reopened index should be rebuilt");

        engine::Session session {reopened};
        execute_ok(session, "USE demo;");
        execute_ok(session, "UPDATE users SET age = 19 WHERE id = 1;");
        require(find_index_equal(reopened, index_id, schema::Value {std::int32_t {18}}).empty(), "persistent UPDATE should remove old index key");
        require(find_index_equal(reopened, index_id, schema::Value {std::int32_t {19}}).size() == 1, "persistent UPDATE should add new index key");
        execute_ok(session, "DELETE FROM users WHERE id = 1;");
        require(find_index_equal(reopened, index_id, schema::Value {std::int32_t {19}}).empty(), "persistent DELETE should remove index key");
        auto dropped = execute_ok(session, "DROP INDEX idx_age ON users;");
        require(dropped.affected_rows == 1, "DROP INDEX affected rows mismatch");
        require(reopened.catalog().find_index(users_id, "idx_age") == nullptr, "dropped index should leave catalog");
        require(!reopened.index_manager().find_index(index_id).has_value(), "dropped index should leave manager");
    }

    {
        engine::DatabaseInstance reopened {engine::DatabaseConfig {.data_dir = dir}};
        const auto * database = reopened.catalog().find_database("demo");
        require(database != nullptr, "second reopen database missing");
        const auto * collection = reopened.catalog().find_collection(database->id(), "users");
        require(collection != nullptr, "second reopen collection missing");
        require(collection->id() == users_id, "second reopen collection id mismatch");
        require(reopened.catalog().find_index(users_id, "idx_age") == nullptr, "dropped index should not reappear");
    }
}

void test_vector_index_ddl_reopen()
{
    const auto dir = make_temp_dir("litedb_vector_index_ddl_reopen_test");

    common::CollectionId docs_id {0};
    {
        engine::DatabaseInstance instance {engine::DatabaseConfig {.data_dir = dir}};
        engine::Session session {instance};
        execute_ok(session, "CREATE DATABASE demo;");
        execute_ok(session, "USE demo;");
        execute_ok(session, "CREATE COLLECTION docs (id BIGINT PRIMARY KEY, embedding VECTOR(3));");

        auto created = execute_ok(
            session,
            "CREATE VINDEX vidx_embedding ON docs (embedding) USING HNSW "
            "WITH (metric = INNER_PRODUCT, max_neighbors = 24, ef_construction = 240, ef_search = 80, random_seed = 7);"
        );
        require(created.affected_rows == 1, "CREATE VINDEX affected rows mismatch");

        const auto * database = instance.catalog().find_database("demo");
        require(database != nullptr, "vector index database lookup failed");
        const auto * collection = instance.catalog().find_collection(database->id(), "docs");
        require(collection != nullptr, "vector index collection lookup failed");
        docs_id = collection->id();

        const auto * index = instance.catalog().find_vector_index(docs_id, "vidx_embedding");
        require(index != nullptr, "created vector index lookup failed");
        require(index->metric() == catalog::CatalogVectorDistanceMetric::InnerProduct, "created vector index metric mismatch");
        require(index->dimension() == 3, "created vector index dimension mismatch");
    }

    {
        engine::DatabaseInstance reopened {engine::DatabaseConfig {.data_dir = dir}};
        const auto * database = reopened.catalog().find_database("demo");
        require(database != nullptr, "reopened vector index database missing");
        const auto * collection = reopened.catalog().find_collection(database->id(), "docs");
        require(collection != nullptr, "reopened vector index collection missing");
        require(collection->id() == docs_id, "reopened vector index collection id mismatch");

        const auto * index = reopened.catalog().find_vector_index(docs_id, "vidx_embedding");
        require(index != nullptr, "reopened vector index missing");
        require(index->index_kind() == catalog::CatalogVectorIndexKind::Hnsw, "reopened vector index kind mismatch");
        require(index->metric() == catalog::CatalogVectorDistanceMetric::InnerProduct, "reopened vector index metric mismatch");
        require(index->dimension() == 3, "reopened vector index dimension mismatch");
        require(index->max_neighbors() == 24, "reopened vector index max_neighbors mismatch");
        require(index->ef_construction() == 240, "reopened vector index ef_construction mismatch");
        require(index->ef_search_default() == 80, "reopened vector index ef_search mismatch");
        require(index->random_seed() == 7, "reopened vector index random_seed mismatch");

        engine::Session session {reopened};
        execute_ok(session, "USE demo;");
        auto existing = execute_ok(session, "CREATE VINDEX IF NOT EXISTS vidx_embedding ON docs (embedding) USING HNSW;");
        require(existing.affected_rows == 0, "persistent CREATE VINDEX IF NOT EXISTS affected rows mismatch");
        auto dropped = execute_ok(session, "DROP VINDEX vidx_embedding ON docs;");
        require(dropped.affected_rows == 1, "persistent DROP VINDEX affected rows mismatch");
        require(reopened.catalog().find_vector_index(docs_id, "vidx_embedding") == nullptr, "dropped vector index should leave catalog");
    }

    {
        engine::DatabaseInstance reopened {engine::DatabaseConfig {.data_dir = dir}};
        const auto * database = reopened.catalog().find_database("demo");
        require(database != nullptr, "second vector index reopen database missing");
        const auto * collection = reopened.catalog().find_collection(database->id(), "docs");
        require(collection != nullptr, "second vector index reopen collection missing");
        require(collection->id() == docs_id, "second vector index reopen collection id mismatch");
        require(reopened.catalog().find_vector_index(docs_id, "vidx_embedding") == nullptr, "dropped vector index should not reappear");
    }
}

void test_drop_collection_reopen()
{
    const auto dir = make_temp_dir("litedb_drop_reopen_test");

    {
        engine::DatabaseInstance instance {engine::DatabaseConfig {.data_dir = dir}};
        engine::Session session {instance};
        execute_ok(session, "CREATE DATABASE demo;");
        execute_ok(session, "USE demo;");
        execute_ok(session, "CREATE COLLECTION users (id BIGINT PRIMARY KEY);");
        execute_ok(session, "INSERT INTO users VALUES (1);");
        execute_ok(session, "DROP COLLECTION users;");
    }

    {
        engine::DatabaseInstance reopened {engine::DatabaseConfig {.data_dir = dir}};
        engine::Session session {reopened};
        execute_ok(session, "USE demo;");
        auto collections = execute_ok(session, "SHOW COLLECTIONS;");
        require(collections.rows.empty(), "dropped collection should not reappear");
    }
}

void test_default_instance_is_still_memory_only()
{
    engine::DatabaseInstance first;
    {
        engine::Session session {first};
        execute_ok(session, "CREATE DATABASE demo;");
    }

    engine::DatabaseInstance second;
    engine::Session session {second};
    auto result = session.execute_sql("USE demo;");
    require(!result.has_value(), "default in-memory instance should not persist data");
}

} // namespace

int main()
{
    try {
        test_binary_value_roundtrip();
        test_manifest_and_catalog_store();
        test_persistent_collection_storage_get();
        test_row_log_replay_and_partial_tail();
        test_database_instance_reopens_persistent_data();
        test_index_ddl_reopen();
        test_vector_index_ddl_reopen();
        test_drop_collection_reopen();
        test_default_instance_is_still_memory_only();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
