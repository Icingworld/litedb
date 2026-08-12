#include "core/database/database_engine.hpp"
#include "core/database/session.hpp"
#include "core/transaction/transaction_manager.hpp"

#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>

#ifndef _WIN32
#include <sys/wait.h>
#endif

namespace
{
using namespace litedb::core;

struct StageCase
{
    transaction::CheckpointStage stage;
    const char * name;
    std::uint64_t expected_generation;
};

constexpr StageCase Stages[] {
    {transaction::CheckpointStage::AfterWalFlush, "after_wal_flush", 1},
    {transaction::CheckpointStage::AfterParticipantSync, "after_participant_sync", 1},
    {transaction::CheckpointStage::AfterTemporaryWalSync, "after_temporary_wal_sync", 1},
    {transaction::CheckpointStage::AfterWalPublish, "after_wal_publish", 2},
    {transaction::CheckpointStage::AfterWalDirectorySync, "after_wal_directory_sync", 2},
    {transaction::CheckpointStage::AfterWalSwitch, "after_wal_switch", 2},
    {transaction::CheckpointStage::AfterOldWalRemoval, "after_old_wal_removal", 2},
};

void require(bool condition, const char * message)
{
    if (!condition) throw std::runtime_error(message);
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

const StageCase * find_stage(std::string_view name)
{
    for (const auto & stage : Stages) {
        if (name == stage.name) return &stage;
    }
    return nullptr;
}

void initialize_database(const std::filesystem::path & directory)
{
    std::filesystem::remove_all(directory);
    auto engine = open_database(directory);
    database::Session session {*engine};
    execute_ok(session, "CREATE DATABASE demo;");
    execute_ok(session, "USE demo;");
    execute_ok(session, "CREATE COLLECTION docs (id BIGINT, body VARCHAR(32));");
    execute_ok(session, "CREATE INDEX doomed ON docs (id) USING BTREE;");
    execute_ok(session, "INSERT INTO docs VALUES (7, 'durable');");
    execute_ok(session, "DROP INDEX doomed ON docs;");
}

int run_worker(const std::filesystem::path & directory, const StageCase & selected)
{
    transaction::TransactionOptions options {
        .checkpoint_stage_hook = [stage = selected.stage](
                                     transaction::CheckpointStage current,
                                     transaction::TransactionId
                                 ) {
            if (current == stage) std::_Exit(87);
        },
    };
    auto engine = open_database(directory, std::move(options));
    auto checkpointed = engine->checkpoint();
    if (!checkpointed) throw std::runtime_error(checkpointed.error().message());
    return 0;
}

void verify_recovered_state(const std::filesystem::path & directory, const StageCase & selected)
{
    auto engine = open_database(directory);
    const auto observation = engine->observability();
    require(observation.transaction.wal_generation == selected.expected_generation,
            "recovery selected an unexpected WAL generation");

    database::Session session {*engine};
    execute_ok(session, "USE demo;");
    const auto rows = execute_ok(session, "SELECT id, body FROM docs;");
    require(rows.rows.size() == 1, "checkpoint crash lost or duplicated a committed row");

    const auto database_entry = engine->catalog().find_database("demo");
    require(database_entry.has_value(), "database metadata missing after checkpoint crash");
    const auto collection = engine->catalog().find_collection(database_entry->id(), "docs");
    require(collection.has_value(), "collection metadata missing after checkpoint crash");
    require(!engine->catalog().find_index(collection->id(), "doomed").has_value(),
            "checkpoint crash resurrected a dropped index");

    if (selected.expected_generation == 2) {
        require(observation.transaction.checkpoint_transaction_id > transaction::InvalidTransactionId,
                "checkpoint transaction boundary was not retained in the new WAL header");
        require(observation.replayed_writes == 0,
                "published checkpoint unexpectedly replayed writes from an older WAL generation");
    }
}

void run_parent(const std::filesystem::path & executable)
{
    for (const auto & stage : Stages) {
        const auto directory = std::filesystem::temp_directory_path() /
                               (std::string("litedb_checkpoint_crash_") + stage.name);
        initialize_database(directory);
        const auto arguments = executable.string() + "\" --worker \"" + directory.string() + "\" " + stage.name;
#ifdef _WIN32
        const auto command = "\"\"" + arguments + "\"";
#else
        const auto command = "\"" + arguments;
#endif
        const auto exit_code = std::system(command.c_str());
#ifdef _WIN32
        require(exit_code == 87, "checkpoint worker did not exit at the injected stage");
#else
        require(WIFEXITED(exit_code) && WEXITSTATUS(exit_code) == 87,
                "checkpoint worker did not exit at the injected stage");
#endif
        verify_recovered_state(directory, stage);
    }
}

} // namespace

int main(int argc, char ** argv)
{
    if (argc == 4 && std::string_view {argv[1]} == "--worker") {
        const auto * stage = find_stage(argv[3]);
        require(stage != nullptr, "unknown checkpoint crash stage");
        return run_worker(argv[2], *stage);
    }
    run_parent(std::filesystem::absolute(argv[0]));
    return 0;
}
