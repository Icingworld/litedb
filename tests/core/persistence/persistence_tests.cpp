#include "core/engine/database_instance.hpp"
#include "core/engine/session.hpp"
#include "core/persistence/binary_io.hpp"
#include "core/persistence/catalog_store.hpp"
#include "core/persistence/manifest_store.hpp"
#include "core/persistence/row_log.hpp"

#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
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
        test_row_log_replay_and_partial_tail();
        test_database_instance_reopens_persistent_data();
        test_drop_collection_reopen();
        test_default_instance_is_still_memory_only();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
