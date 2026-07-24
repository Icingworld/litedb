#include "core/database/database_engine.hpp"
#include "core/database/session.hpp"
#include "core/filesystem/platform_filesystem.hpp"
#include "core/wal/wal_manager.hpp"

#include <filesystem>
#include <stdexcept>
#include <string_view>

namespace
{
using namespace litedb::core;

void require(bool condition, const char * message)
{
    if (!condition) throw std::runtime_error(message);
}

executor::ExecutionResult execute_ok(database::Session & session, std::string_view sql)
{
    auto result = session.execute_sql(sql);
    if (!result) throw std::runtime_error(result.error().message);
    return std::move(*result);
}

std::unique_ptr<database::DatabaseEngine> open_database(const std::filesystem::path & directory)
{
    auto opened = database::DatabaseEngine::open({.data_dir = directory});
    if (!opened) throw std::runtime_error(opened.error().message);
    return std::move(*opened);
}

std::unique_ptr<database::DatabaseEngine> open_database(database::DatabaseConfig config)
{
    auto opened = database::DatabaseEngine::open(std::move(config));
    if (!opened) throw std::runtime_error(opened.error().message);
    return std::move(*opened);
}

void test_automatic_checkpoint_by_wal_size()
{
    const auto directory = std::filesystem::temp_directory_path() / "litedb_automatic_checkpoint_tests";
    std::filesystem::remove_all(directory);

    {
        auto engine = open_database(database::DatabaseConfig {
            .data_dir = directory,
            .automatic_checkpoint = {
                .wal_size_threshold_bytes = wal::WalCodec::FileHeaderSize + 1,
            },
        });
        database::Session session {*engine};
        execute_ok(session, "CREATE DATABASE demo;");
        execute_ok(session, "USE demo;");
        execute_ok(session, "CREATE COLLECTION docs (id BIGINT);");
        execute_ok(session, "INSERT INTO docs VALUES (1);");

        const auto observation = engine->observability();
        require(observation.automatic_checkpoint_attempts == 3,
                "automatic checkpoint attempt count mismatch");
        require(observation.completed_automatic_checkpoints == 3 &&
                observation.failed_automatic_checkpoints == 0,
                "automatic checkpoint completion count mismatch");
        require(observation.transaction.completed_checkpoints == 3,
                "transaction checkpoint metrics did not include automatic checkpoints");
        require(observation.transaction.wal_generation == 4,
                "automatic checkpoint did not rotate the WAL after each write statement");
        require(observation.transaction.wal_size_bytes == wal::WalCodec::FileHeaderSize,
                "automatic checkpoint did not reclaim the WAL");
    }

    auto reopened = open_database(directory);
    database::Session session {*reopened};
    execute_ok(session, "USE demo;");
    const auto rows = execute_ok(session, "SELECT id FROM docs;");
    require(rows.rows.size() == 1, "automatic checkpoint restart lost committed data");
}

void test_database_recovery_uses_configured_wal_limits()
{
    const auto directory = std::filesystem::temp_directory_path() / "litedb_wal_limit_config_tests";
    std::filesystem::remove_all(directory);
    {
        auto engine = open_database(directory);
        database::Session session {*engine};
        execute_ok(session, "CREATE DATABASE demo;");
    }

    auto limited = database::DatabaseEngine::open(database::DatabaseConfig {
        .data_dir = directory,
        .wal_decode_limits = {
            .max_record_size_bytes = 1024 * 1024,
            .max_scan_size_bytes = wal::WalCodec::FileHeaderSize,
            .max_record_count = 16,
        },
    });
    require(!limited && limited.error().code == database::DatabaseErrorCode::WalError,
            "Database recovery did not enforce configured WAL limits");
}

void test_wal_write_budget_prevents_unrecoverable_commit()
{
    const auto directory = std::filesystem::temp_directory_path() / "litedb_wal_write_budget_tests";
    std::filesystem::remove_all(directory);
    const database::DatabaseConfig config {
        .data_dir = directory,
        .wal_decode_limits = {
            .max_record_size_bytes = 1024 * 1024,
            .max_scan_size_bytes = 1024 * 1024,
            .max_record_count = 4,
        },
    };
    {
        auto engine = open_database(config);
        database::Session session {*engine};
        execute_ok(session, "CREATE DATABASE demo;");
        execute_ok(session, "USE demo;");
        const auto rejected = session.execute_sql("CREATE COLLECTION docs (id BIGINT);");
        require(!rejected, "WAL budget should reject a transaction that cannot be recovered");
    }

    auto reopened = open_database(config);
    database::Session session {*reopened};
    execute_ok(session, "USE demo;");
    const auto collections = execute_ok(session, "SHOW COLLECTIONS;");
    require(collections.rows.empty(), "rejected over-budget transaction became visible after restart");
}

} // namespace

int main()
{
    const auto directory = std::filesystem::temp_directory_path() / "litedb_checkpoint_tests";
    std::filesystem::remove_all(directory);

    transaction::TransactionId checkpoint_transaction_id = transaction::InvalidTransactionId;
    {
        auto engine = open_database(directory);
        database::Session session {*engine};
        execute_ok(session, "CREATE DATABASE demo;");
        execute_ok(session, "USE demo;");
        execute_ok(session, "CREATE COLLECTION docs (id BIGINT);");
        execute_ok(session, "INSERT INTO docs VALUES (1);");

        const auto before = engine->observability().transaction;
        require(before.wal_generation == 1 && before.wal_size_bytes > wal::WalCodec::FileHeaderSize,
                "initial WAL observation mismatch");
        const auto automatic_before = engine->observability();
        require(automatic_before.automatic_checkpoint_attempts == 0,
                "automatic checkpoint should be disabled by default");
        require(engine->checkpoint().has_value(), "manual checkpoint failed");

        const auto after = engine->observability().transaction;
        require(after.wal_generation == 2, "checkpoint did not advance WAL generation");
        require(after.completed_checkpoints == 1 && after.failed_checkpoints == 0,
                "checkpoint counters mismatch");
        require(after.wal_size_bytes == wal::WalCodec::FileHeaderSize && after.reclaimed_wal_bytes > 0,
                "checkpoint WAL reclamation metrics mismatch");
        require(after.checkpoint_transaction_id > transaction::InvalidTransactionId,
                "checkpoint transaction ID was not published");
        checkpoint_transaction_id = after.checkpoint_transaction_id;

        execute_ok(session, "INSERT INTO docs VALUES (2);");
    }

    auto filesystem = filesystem::create_platform_filesystem();
    auto wal = wal::WalManager::open(directory / "wal", filesystem);
    require(wal.has_value(), "reopen segmented WAL failed");
    auto scanned = wal->scan(false);
    require(scanned.has_value(), "scan post-checkpoint WAL failed");
    require(scanned->maximum_transaction_id > checkpoint_transaction_id,
            "transaction IDs did not remain monotonic after checkpoint");

    auto engine = open_database(directory);
    database::Session session {*engine};
    execute_ok(session, "USE demo;");
    const auto rows = execute_ok(session, "SELECT id FROM docs;");
    require(rows.rows.size() == 2, "checkpoint restart lost committed rows");
    test_automatic_checkpoint_by_wal_size();
    test_database_recovery_uses_configured_wal_limits();
    test_wal_write_budget_prevents_unrecoverable_commit();
    return 0;
}
