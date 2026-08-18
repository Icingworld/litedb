#include "core/database/database_engine.hpp"
#include "core/database/session.hpp"
#include "core/filesystem/platform_filesystem.hpp"
#include "core/index/scalar_index_key.hpp"
#include "core/vindex/vector_index_key.hpp"
#include "core/wal/wal_codec.hpp"
#include "core/wal/wal_store.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <variant>
#include <vector>

namespace
{
using namespace litedb::core;

constexpr std::size_t StoragePageSize = 4096;

void require(bool condition, const char * message)
{
    if (!condition) throw std::runtime_error(message);
}

std::filesystem::path temp_dir(std::string name)
{
    auto path = std::filesystem::temp_directory_path() / std::move(name);
    std::filesystem::remove_all(path);
    std::filesystem::create_directories(path);
    return path;
}

std::unique_ptr<database::DatabaseEngine> open_database(
    const std::filesystem::path & path,
    transaction::TransactionOptions options = {}
)
{
    auto opened = database::DatabaseEngine::open({
        .data_dir = path,
        .transaction_options = std::move(options),
    });
    if (!opened) throw std::runtime_error(opened.error().message());
    return std::move(*opened);
}

executor::ExecutionResult execute_ok(database::Session & session, std::string_view sql)
{
    auto result = session.execute_sql(sql);
    if (!result) throw std::runtime_error(result.error().message());
    return std::move(*result);
}

std::vector<std::byte> read_bytes(const std::filesystem::path & path)
{
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    require(static_cast<bool>(input), "open snapshot file failed");
    const auto size = input.tellg();
    require(size >= 0, "size snapshot file failed");
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    input.seekg(0);
    if (!bytes.empty()) require(static_cast<bool>(input.read(reinterpret_cast<char *>(bytes.data()), size)), "read snapshot failed");
    return bytes;
}

void write_bytes(const std::filesystem::path & path, const std::vector<std::byte> & bytes)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(output), "open snapshot restore failed");
    if (!bytes.empty()) output.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    require(static_cast<bool>(output), "write snapshot restore failed");
}

void test_multi_row_update_is_atomic()
{
    const auto directory = temp_dir("litedb_transaction_atomic_update");
    auto engine = open_database(directory);
    database::Session session {*engine};
    execute_ok(session, "CREATE DATABASE demo;");
    execute_ok(session, "USE demo;");
    execute_ok(session, "CREATE COLLECTION users (id BIGINT UNIQUE, name VARCHAR(32), embedding VECTOR(2));");
    execute_ok(session, "CREATE INDEX idx_id ON users (id) USING BTREE;");
    execute_ok(session, "CREATE VINDEX vidx_embedding ON users (embedding) USING HNSW;");
    execute_ok(session, "INSERT INTO users VALUES (1, 'one', [1.0, 0.0]);");
    execute_ok(session, "INSERT INTO users VALUES (2, 'two', [0.0, 1.0]);");

    // Row 1 can be evaluated and staged, while row 2 divides by zero. No
    // participant may observe the first staged row when the statement fails.
    auto failed = session.execute_sql("UPDATE users SET id = 10 / (id - 2);");
    require(!failed, "multi-row update should fail on the second row");
    auto selected = execute_ok(session, "SELECT id FROM users ORDER BY id ASC;");
    require(selected.rows.size() == 2, "failed transaction changed row count");
    require(std::get<std::int64_t>(selected.rows[0].values[0].data()) == 1, "failed transaction changed first row");
    require(std::get<std::int64_t>(selected.rows[1].values[0].data()) == 2, "failed transaction changed second row");
}

void test_committed_wal_redoes_all_participants_and_ignores_loser()
{
    const auto directory = temp_dir("litedb_transaction_redo");
    common::CollectionId collection_id {0};
    common::IndexId index_id {0};
    common::VIndexId vector_index_id {0};
    std::vector<std::byte> collection_before;
    std::vector<std::byte> index_before;
    std::vector<std::byte> vector_before;

    {
        auto engine = open_database(directory);
        database::Session session {*engine};
        execute_ok(session, "CREATE DATABASE demo;");
        execute_ok(session, "USE demo;");
        execute_ok(session, "CREATE COLLECTION docs (id BIGINT UNIQUE, embedding VECTOR(2));");
        execute_ok(session, "CREATE INDEX idx_id ON docs (id) USING BTREE;");
        execute_ok(session, "CREATE VINDEX vidx_embedding ON docs (embedding) USING HNSW;");
        const auto database = engine->catalog().find_database("demo");
        require(database.has_value(), "database missing");
        const auto collection = engine->catalog().find_collection(database->id(), "docs");
        require(collection.has_value(), "collection missing");
        collection_id = collection->id();
        index_id = engine->catalog().find_index(collection_id, "idx_id")->id();
        vector_index_id = engine->catalog().find_vector_index(collection_id, "vidx_embedding")->id();
    }

    const auto collection_path = directory / "collections" / (std::to_string(collection_id) + ".store");
    const auto index_path = directory / "indexes" / (std::to_string(index_id) + ".bti");
    const auto vector_path = directory / "vindexes" / ("vindex_" + std::to_string(vector_index_id) + ".lhnsw");
    collection_before = read_bytes(collection_path);
    index_before = read_bytes(index_path);
    vector_before = read_bytes(vector_path);

    {
        auto engine = open_database(directory);
        database::Session session {*engine};
        execute_ok(session, "USE demo;");
        execute_ok(session, "INSERT INTO docs VALUES (7, [1.0, 0.0]);");
    }

    // Simulate losing every participant write after the WAL commit became durable.
    write_bytes(collection_path, collection_before);
    write_bytes(index_path, index_before);
    write_bytes(vector_path, vector_before);

    {
        auto filesystem = filesystem::create_platform_filesystem();
        auto wal = wal::WalManager::open(directory / "wal", filesystem);
        require(wal.has_value(), "open WAL for loser transaction failed");
        auto begin = wal->append_begin(999);
        require(begin.has_value(), "append loser begin failed");
        wal::FileWrite poison {
            .target = {.kind = wal::FileKind::CollectionStore, .object_id = collection_id},
            .offset = 0,
            .after_image = std::vector<std::byte>(collection_before.size(), std::byte {0}),
        };
        auto write = wal->append_write(999, poison);
        require(write.has_value(), "append loser write failed");
        require(wal->flush_through(*write).has_value(), "flush loser write failed");
    }

    auto recovered = open_database(directory);
    database::Session session {*recovered};
    execute_ok(session, "USE demo;");
    auto selected = execute_ok(session, "SELECT id FROM docs;");
    require(selected.rows.size() == 1 && std::get<std::int64_t>(selected.rows[0].values[0].data()) == 7,
            "committed storage write was not recovered or loser was replayed");

    auto scalar_key = index::ScalarIndexKey::from_value(common::Value {std::int64_t {7}});
    require(scalar_key.has_value(), "scalar key failed");
    auto scalar = recovered->index_engine().find_equal(index_id, *scalar_key);
    require(scalar && scalar->size() == 1, "scalar index WAL redo failed");

    auto vector_key = vindex::VectorIndexKey::from_vector({1.0, 0.0});
    require(vector_key.has_value(), "vector key failed");
    auto vector = recovered->vector_index_engine().search(vector_index_id, *vector_key, {.top_k = 1});
    require(vector && vector->size() == 1 && (*vector)[0].record_id == (*scalar)[0], "vector index WAL redo failed");
    recovered.reset();

    auto recovered_twice = open_database(directory);
    database::Session second_session {*recovered_twice};
    execute_ok(second_session, "USE demo;");
    auto second_selected = execute_ok(second_session, "SELECT id FROM docs;");
    require(second_selected.rows.size() == 1 &&
            std::get<std::int64_t>(second_selected.rows[0].values[0].data()) == 7,
            "WAL redo is not idempotent across repeated restart");
}

void test_failpoint_metrics_and_staging_cleanup()
{
    const auto directory = temp_dir("litedb_transaction_observability");
    {
        auto engine = open_database(directory);
        database::Session session {*engine};
        execute_ok(session, "CREATE DATABASE demo;");
        execute_ok(session, "USE demo;");
        execute_ok(session, "CREATE COLLECTION events (id BIGINT);");
    }

    std::filesystem::create_directories(directory / ".transactions" / "txn_stale");
    {
        std::ofstream marker(directory / ".transactions" / "txn_stale" / "marker");
        marker << "stale";
    }

    bool injected = false;
    transaction::TransactionOptions options {
        .commit_stage_hook = [&injected](transaction::CommitStage stage, transaction::TransactionId) {
            if (!injected && stage == transaction::CommitStage::AfterPrepare) {
                injected = true;
                return true;
            }
            return false;
        },
    };
    auto engine = open_database(directory, std::move(options));
    require(!std::filesystem::exists(directory / ".transactions"), "startup did not clean stale staging");
    database::Session session {*engine};
    execute_ok(session, "USE demo;");
    auto failed = session.execute_sql("INSERT INTO events VALUES (1);");
    require(!failed, "injected prepare failure did not fail the transaction");
    execute_ok(session, "INSERT INTO events VALUES (2);");

    const auto observation = engine->observability();
    require(observation.transaction.started_transactions == 2, "started transaction metric mismatch");
    require(observation.transaction.committed_transactions == 1, "committed transaction metric mismatch");
    require(observation.transaction.aborted_transactions == 1, "aborted transaction metric mismatch");
    require(observation.transaction.failed_commits == 1, "failed commit metric mismatch");
    require(observation.transaction.wal_size_bytes > wal::WalCodec::FileHeaderSize, "WAL size metric mismatch");
    require(observation.transaction.maximum_commit_duration_us >= observation.transaction.last_commit_duration_us,
            "transaction duration metrics mismatch");
    engine.reset();

    auto recovered = open_database(directory);
    const auto recovery_observation = recovered->observability();
    require(recovery_observation.recovered_committed_transactions >= 1,
            "recovered transaction metric mismatch");
    require(recovery_observation.replayed_writes >= 1, "redo write metric mismatch");
}

void test_dml_staging_is_scoped_to_affected_collection()
{
    const auto directory = temp_dir("litedb_transaction_scoped_staging");
    common::CollectionId users_id {0};
    common::CollectionId audit_id {0};
    common::IndexId users_index_id {0};
    common::IndexId audit_index_id {0};
    common::VIndexId users_vector_id {0};
    common::VIndexId audit_vector_id {0};
    {
        auto engine = open_database(directory);
        database::Session session {*engine};
        execute_ok(session, "CREATE DATABASE demo;");
        execute_ok(session, "USE demo;");
        execute_ok(session, "CREATE COLLECTION users (id BIGINT, embedding VECTOR(2));");
        execute_ok(session, "CREATE COLLECTION audit (id BIGINT, embedding VECTOR(2));");
        execute_ok(session, "CREATE INDEX idx_users_id ON users (id) USING BTREE;");
        execute_ok(session, "CREATE INDEX idx_audit_id ON audit (id) USING BTREE;");
        execute_ok(session, "CREATE VINDEX vidx_users ON users (embedding) USING HNSW;");
        execute_ok(session, "CREATE VINDEX vidx_audit ON audit (embedding) USING HNSW;");
        execute_ok(session, "INSERT INTO users VALUES (1, [1.0, 0.0]);");
        execute_ok(session, "INSERT INTO audit VALUES (2, [0.0, 1.0]);");

        const auto database_entry = engine->catalog().find_database("demo");
        require(database_entry.has_value(), "scoped staging database missing");
        const auto users = engine->catalog().find_collection(database_entry->id(), "users");
        const auto audit = engine->catalog().find_collection(database_entry->id(), "audit");
        require(users.has_value() && audit.has_value(), "scoped staging collections missing");
        users_id = users->id();
        audit_id = audit->id();
        users_index_id = engine->catalog().find_index(users_id, "idx_users_id")->id();
        audit_index_id = engine->catalog().find_index(audit_id, "idx_audit_id")->id();
        users_vector_id = engine->catalog().find_vector_index(users_id, "vidx_users")->id();
        audit_vector_id = engine->catalog().find_vector_index(audit_id, "vidx_audit")->id();
    }

    bool inspected = false;
    transaction::TransactionOptions options {
        .commit_stage_hook = [&](transaction::CommitStage stage, transaction::TransactionId transaction_id) {
            if (stage != transaction::CommitStage::AfterPrepare) return false;
            inspected = true;
            const auto staging = directory / ".transactions" / ("txn_" + std::to_string(transaction_id));
            require(!std::filesystem::exists(staging),
                    "sparse prepare created a physical transaction staging directory");
            require(!std::filesystem::exists(staging / "collections" / (std::to_string(audit_id) + ".store")),
                    "unrelated collection was staged");
            require(!std::filesystem::exists(staging / "indexes" / (std::to_string(audit_index_id) + ".bti")),
                    "unrelated scalar index was staged");
            require(!std::filesystem::exists(staging / "vindexes" / ("vindex_" + std::to_string(audit_vector_id) + ".lhnsw")),
                    "unrelated vector index was staged");
            return false;
        },
    };
    auto engine = open_database(directory, std::move(options));
    database::Session session {*engine};
    execute_ok(session, "USE demo;");
    execute_ok(session, "UPDATE users SET id = 3 WHERE id = 1;");
    require(inspected, "scoped staging hook was not reached");

    auto users = execute_ok(session, "SELECT id FROM users;");
    auto audit = execute_ok(session, "SELECT id FROM audit;");
    require(users.rows.size() == 1 && std::get<std::int64_t>(users.rows[0].values[0].data()) == 3,
            "affected collection runtime was not refreshed");
    require(audit.rows.size() == 1 && std::get<std::int64_t>(audit.rows[0].values[0].data()) == 2,
            "unrelated collection runtime changed");
}

void test_single_row_wal_write_amplification()
{
    const auto directory = temp_dir("litedb_transaction_write_amplification");
    common::CollectionId collection_id {0};
    {
        auto engine = open_database(directory);
        database::Session session {*engine};
        execute_ok(session, "CREATE DATABASE demo;");
        execute_ok(session, "USE demo;");
        execute_ok(session, "CREATE COLLECTION docs (id BIGINT, payload VARCHAR(3500));");
        const auto large = std::string(2500, 'a');
        for (std::int64_t id = 1; id <= 6; ++id) {
            execute_ok(session, "INSERT INTO docs VALUES (" + std::to_string(id) + ", '" + large + "');");
        }
        const auto database_entry = engine->catalog().find_database("demo");
        require(database_entry.has_value(), "write amplification database missing");
        const auto collection = engine->catalog().find_collection(database_entry->id(), "docs");
        require(collection.has_value(), "write amplification collection missing");
        collection_id = collection->id();
        execute_ok(session, "UPDATE docs SET payload = '" + std::string(2000, 'b') + "' WHERE id = 3;");
    }

    auto filesystem = filesystem::create_platform_filesystem();
    auto manager = wal::WalManager::open(directory / "wal", filesystem);
    require(manager.has_value(), "write amplification WAL open failed");
    auto scanned = manager->scan(false);
    require(scanned.has_value(), "write amplification WAL scan failed");
    transaction::TransactionId final_transaction {0};
    for (const auto & record : scanned->records) {
        if (record.type == wal::WalRecordType::Commit) {
            final_transaction = std::max(final_transaction, record.transaction_id);
        }
    }
    require(final_transaction != 0, "final committed transaction was not found");

    std::size_t storage_after_image_bytes {0};
    bool storage_replace {false};
    std::size_t storage_writes {0};
    for (const auto & record : scanned->records) {
        if (record.transaction_id != final_transaction ||
            record.type != wal::WalRecordType::FileWrite) {
            continue;
        }
        auto write = wal::WalCodec::decode_file_write(record.payload);
        require(write.has_value(), "write amplification WAL payload decode failed");
        if (write->target.kind != wal::FileKind::CollectionStore ||
            write->target.object_id != collection_id) {
            continue;
        }
        ++storage_writes;
        storage_after_image_bytes += write->after_image.size();
        storage_replace = storage_replace || write->mode == wal::FileWriteMode::Replace;
    }
    require(storage_writes != 0, "single-row update emitted no storage WAL");
    require(!storage_replace, "single-row update emitted a full collection replacement");
    require(storage_after_image_bytes <= 3 * StoragePageSize,
            "single-row storage WAL exceeded header plus two data pages");
}
}

int main()
{
    test_multi_row_update_is_atomic();
    test_committed_wal_redoes_all_participants_and_ignores_loser();
    test_failpoint_metrics_and_staging_cleanup();
    test_dml_staging_is_scoped_to_affected_collection();
    test_single_row_wal_write_amplification();
    return 0;
}
