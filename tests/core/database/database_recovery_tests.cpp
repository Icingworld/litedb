#include "core/database/database_engine.hpp"
#include "core/database/database_manifest.hpp"
#include "core/database/session.hpp"
#include "core/filesystem/platform_filesystem.hpp"
#include "core/index/scalar_index_key.hpp"
#include "core/storage/storage_error.hpp"

#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
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
    database::DatabaseEngine & engine,
    common::IndexId index_id,
    schema::Value value
)
{
    auto key = index::ScalarIndexKey::from_value(std::move(value));
    require(key.has_value(), "index key creation failed");

    auto index_view = engine.index_engine().find_index(index_id);
    require(index_view.has_value(), "managed index missing");

    auto found = engine.index_engine().find_equal(index_id, key.value());
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

std::unique_ptr<database::DatabaseEngine> open_database(const std::filesystem::path & data_dir)
{
    auto opened = database::DatabaseEngine::open(database::DatabaseConfig {.data_dir = data_dir});
    if (!opened.has_value()) {
        throw std::runtime_error(opened.error().message);
    }
    return std::move(opened.value());
}

executor::ExecutionResult execute_ok(database::Session & session, std::string_view sql)
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

void test_database_manifest()
{
    const auto dir = make_temp_dir("litedb_manifest_catalog_test");
    auto filesystem = filesystem::create_platform_filesystem();
    database::DatabaseManifest manifest {dir, filesystem};
    auto initialized = manifest.ensure_initialized();
    require(initialized.has_value(), "manifest init failed");
    require(std::filesystem::exists(dir / "manifest.ldb"), "manifest file missing");
    require(std::filesystem::exists(dir / "collections"), "collections dir missing");
}

void test_truncated_database_manifest_is_rejected()
{
    const auto dir = make_temp_dir("litedb_invalid_manifest_test");
    auto filesystem = filesystem::create_platform_filesystem();
    database::DatabaseManifest manifest {dir, filesystem};
    require(manifest.ensure_initialized().has_value(), "manifest init failed");

    {
        std::ofstream output {dir / "manifest.ldb", std::ios::binary | std::ios::trunc};
        const char truncated_magic[2] {0, 0};
        output.write(truncated_magic, sizeof(truncated_magic));
    }

    auto reopened = manifest.ensure_initialized();
    require(!reopened.has_value(), "truncated manifest should be rejected");
    require(reopened.error().code == database::ManifestErrorCode::InvalidFormat, "truncated manifest error code mismatch");
}

void test_database_engine_open_propagates_manifest_error()
{
    const auto dir = make_temp_dir("litedb_engine_invalid_manifest_test");
    auto filesystem = filesystem::create_platform_filesystem();
    database::DatabaseManifest manifest {dir, filesystem};
    require(manifest.ensure_initialized().has_value(), "manifest init failed");

    {
        std::ofstream output {dir / "manifest.ldb", std::ios::binary | std::ios::trunc};
        const char truncated_magic[2] {0, 0};
        output.write(truncated_magic, sizeof(truncated_magic));
    }

    auto opened = database::DatabaseEngine::open(database::DatabaseConfig {.data_dir = dir});
    require(!opened.has_value(), "database engine should reject truncated manifest");
    require(
        opened.error().code == database::DatabaseErrorCode::ManifestError,
        "database engine manifest error code mismatch"
    );
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

void test_database_engine_reopens_persistent_data()
{
    const auto dir = make_temp_dir("litedb_reopen_test");

    {
        auto engine = open_database(dir);
        database::Session session {*engine};
        execute_ok(session, "CREATE DATABASE demo;");
        execute_ok(session, "USE demo;");
        execute_ok(
            session,
            "CREATE COLLECTION users ("
            "id BIGINT NOT NULL, "
            "name VARCHAR(64) COMMENT 'display name' DEFAULT 'unknown', "
            "age INTEGER, "
            "embedding VECTOR(3)"
            ") COMMENT 'user collection';"
        );
        execute_ok(session, "INSERT INTO users VALUES (1, 'alice', 18, [0.1, 0.2, 0.3]);");
        execute_ok(session, "INSERT INTO users VALUES (2, 'bob', 20, [0.2, 0.3, 0.4]);");
        execute_ok(session, "UPDATE users SET age = age + 1 WHERE id = 1;");
        execute_ok(session, "DELETE FROM users WHERE id = 2;");
    }

    {
        auto reopened = open_database(dir);
        database::Session session {*reopened};
        execute_ok(session, "USE demo;");
        auto selected = execute_ok(session, "SELECT name, age, embedding FROM users WHERE id = 1;");
        require(selected.kind == executor::ExecutionResultKind::RowSet, "reopen select result kind mismatch");
        require(selected.rows.size() == 1, "reopen row count mismatch");
        require(get_value<std::string>(selected.rows[0].values[0]) == "alice", "reopen name mismatch");
        require(get_value<std::int32_t>(selected.rows[0].values[1]) == 19, "reopen updated age mismatch");
        require(get_value<schema::VectorValue>(selected.rows[0].values[2]).size() == 3, "reopen vector mismatch");

        auto describe = execute_ok(session, "DESCRIBE users;");
        require(get_value<std::string>(describe.rows[1].values[4]) == "display name", "reopen column comment mismatch");
        require(get_value<std::string>(describe.rows[1].values[5]) == "user collection", "reopen collection comment mismatch");

        auto remaining = execute_ok(session, "SELECT id FROM users ORDER BY id ASC;");
        require(remaining.rows.size() == 1, "deleted row should not reappear");
    }
}

void test_index_ddl_reopen()
{
    const auto dir = make_temp_dir("litedb_index_ddl_reopen_test");
    common::IndexId persisted_index_id {0};

    {
        auto engine = open_database(dir);
        database::Session session {*engine};
        execute_ok(session, "CREATE DATABASE demo;");
        execute_ok(session, "USE demo;");
        execute_ok(session, "CREATE COLLECTION users (id BIGINT NOT NULL, age INTEGER);");
        execute_ok(session, "INSERT INTO users VALUES (1, 18);");
        auto created = execute_ok(session, "CREATE INDEX idx_age ON users (age) USING BTREE;");
        require(created.affected_rows == 1, "CREATE INDEX affected rows mismatch");

        const auto * database = engine->meta().find_database("demo");
        require(database != nullptr, "created database lookup failed");
        const auto * collection = engine->meta().find_collection(database->id(), "users");
        require(collection != nullptr, "created collection lookup failed");
        const auto * index = engine->meta().find_index(collection->id(), "idx_age");
        require(index != nullptr, "created index lookup failed");
        persisted_index_id = index->id();
        require(index->kind() == meta::entry::IndexKind::BTree, "created index kind mismatch");
        require(find_index_equal(*engine, index->id(), schema::Value {std::int32_t {18}}).size() == 1, "created index should include existing row");
        require(
            std::filesystem::exists(dir / "indexes" / (std::to_string(persisted_index_id) + ".bti")),
            "created BTREE index file missing"
        );
    }

    common::CollectionId users_id {0};
    {
        auto reopened = open_database(dir);
        const auto * database = reopened->meta().find_database("demo");
        require(database != nullptr, "reopened database missing");
        const auto * collection = reopened->meta().find_collection(database->id(), "users");
        require(collection != nullptr, "reopened collection missing");
        users_id = collection->id();
        const auto * index = reopened->meta().find_index(users_id, "idx_age");
        require(index != nullptr, "reopened index missing");
        require(index->kind() == meta::entry::IndexKind::BTree, "reopened index kind mismatch");
        const auto index_id = index->id();
        require(find_index_equal(*reopened, index_id, schema::Value {std::int32_t {18}}).size() == 1, "reopened persistent index lookup mismatch");

        database::Session session {*reopened};
        execute_ok(session, "USE demo;");
        execute_ok(session, "UPDATE users SET age = 19 WHERE id = 1;");
        require(find_index_equal(*reopened, index_id, schema::Value {std::int32_t {18}}).empty(), "persistent UPDATE should remove old index key");
        require(find_index_equal(*reopened, index_id, schema::Value {std::int32_t {19}}).size() == 1, "persistent UPDATE should add new index key");
        execute_ok(session, "DELETE FROM users WHERE id = 1;");
        require(find_index_equal(*reopened, index_id, schema::Value {std::int32_t {19}}).empty(), "persistent DELETE should remove index key");
        auto dropped = execute_ok(session, "DROP INDEX idx_age ON users;");
        require(dropped.affected_rows == 1, "DROP INDEX affected rows mismatch");
        require(reopened->meta().find_index(users_id, "idx_age") == nullptr, "dropped index should leave catalog");
        require(!reopened->index_engine().find_index(index_id).has_value(), "dropped index should leave engine");
        require(
            !std::filesystem::exists(dir / "indexes" / (std::to_string(index_id) + ".bti")),
            "dropped BTREE index file should be removed"
        );
    }

    {
        auto reopened = open_database(dir);
        const auto * database = reopened->meta().find_database("demo");
        require(database != nullptr, "second reopen database missing");
        const auto * collection = reopened->meta().find_collection(database->id(), "users");
        require(collection != nullptr, "second reopen collection missing");
        require(collection->id() == users_id, "second reopen collection id mismatch");
        require(reopened->meta().find_index(users_id, "idx_age") == nullptr, "dropped index should not reappear");
    }
}

void test_vector_index_ddl_reopen()
{
    const auto dir = make_temp_dir("litedb_vector_index_ddl_reopen_test");

    common::CollectionId docs_id {0};
    {
        auto engine = open_database(dir);
        database::Session session {*engine};
        execute_ok(session, "CREATE DATABASE demo;");
        execute_ok(session, "USE demo;");
        execute_ok(session, "CREATE COLLECTION docs (id BIGINT NOT NULL, embedding VECTOR(3));");

        auto created = execute_ok(
            session,
            "CREATE VINDEX vidx_embedding ON docs (embedding) USING HNSW "
            "WITH (metric = INNER_PRODUCT, max_neighbors = 24, ef_construction = 240, ef_search = 80, random_seed = 7);"
        );
        require(created.affected_rows == 1, "CREATE VINDEX affected rows mismatch");

        const auto * database = engine->meta().find_database("demo");
        require(database != nullptr, "vector index database lookup failed");
        const auto * collection = engine->meta().find_collection(database->id(), "docs");
        require(collection != nullptr, "vector index collection lookup failed");
        docs_id = collection->id();

        const auto * index = engine->meta().find_vector_index(docs_id, "vidx_embedding");
        require(index != nullptr, "created vector index lookup failed");
        require(index->metric() == meta::entry::VectorDistanceMetric::InnerProduct, "created vector index metric mismatch");
        require(index->dimension() == 3, "created vector index dimension mismatch");
    }

    {
        auto reopened = open_database(dir);
        const auto * database = reopened->meta().find_database("demo");
        require(database != nullptr, "reopened vector index database missing");
        const auto * collection = reopened->meta().find_collection(database->id(), "docs");
        require(collection != nullptr, "reopened vector index collection missing");
        require(collection->id() == docs_id, "reopened vector index collection id mismatch");

        const auto * index = reopened->meta().find_vector_index(docs_id, "vidx_embedding");
        require(index != nullptr, "reopened vector index missing");
        require(index->index_kind() == meta::entry::VectorIndexKind::Hnsw, "reopened vector index kind mismatch");
        require(index->metric() == meta::entry::VectorDistanceMetric::InnerProduct, "reopened vector index metric mismatch");
        require(index->dimension() == 3, "reopened vector index dimension mismatch");
        require(index->max_neighbors() == 24, "reopened vector index max_neighbors mismatch");
        require(index->ef_construction() == 240, "reopened vector index ef_construction mismatch");
        require(index->ef_search_default() == 80, "reopened vector index ef_search mismatch");
        require(index->random_seed() == 7, "reopened vector index random_seed mismatch");

        database::Session session {*reopened};
        execute_ok(session, "USE demo;");
        auto existing = execute_ok(session, "CREATE VINDEX IF NOT EXISTS vidx_embedding ON docs (embedding) USING HNSW;");
        require(existing.affected_rows == 0, "persistent CREATE VINDEX IF NOT EXISTS affected rows mismatch");
        auto dropped = execute_ok(session, "DROP VINDEX vidx_embedding ON docs;");
        require(dropped.affected_rows == 1, "persistent DROP VINDEX affected rows mismatch");
        require(reopened->meta().find_vector_index(docs_id, "vidx_embedding") == nullptr, "dropped vector index should leave catalog");
    }

    {
        auto reopened = open_database(dir);
        const auto * database = reopened->meta().find_database("demo");
        require(database != nullptr, "second vector index reopen database missing");
        const auto * collection = reopened->meta().find_collection(database->id(), "docs");
        require(collection != nullptr, "second vector index reopen collection missing");
        require(collection->id() == docs_id, "second vector index reopen collection id mismatch");
        require(reopened->meta().find_vector_index(docs_id, "vidx_embedding") == nullptr, "dropped vector index should not reappear");
    }
}

void test_drop_collection_reopen()
{
    const auto dir = make_temp_dir("litedb_drop_reopen_test");

    {
        auto engine = open_database(dir);
        database::Session session {*engine};
        execute_ok(session, "CREATE DATABASE demo;");
        execute_ok(session, "USE demo;");
        execute_ok(session, "CREATE COLLECTION users (id BIGINT NOT NULL);");
        execute_ok(session, "INSERT INTO users VALUES (1);");
        execute_ok(session, "DROP COLLECTION users;");
    }

    {
        auto reopened = open_database(dir);
        database::Session session {*reopened};
        execute_ok(session, "USE demo;");
        auto collections = execute_ok(session, "SHOW COLLECTIONS;");
        require(collections.rows.empty(), "dropped collection should not reappear");
    }
}

} // namespace

int main()
{
    try {
        test_database_manifest();
        test_truncated_database_manifest_is_rejected();
        test_database_engine_open_propagates_manifest_error();
        test_database_engine_reopens_persistent_data();
        test_index_ddl_reopen();
        test_vector_index_ddl_reopen();
        test_drop_collection_reopen();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
