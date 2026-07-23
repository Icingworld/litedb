#include "core/database/database_engine.hpp"
#include "core/database/session.hpp"
#include "core/index/scalar_index_key.hpp"
#include "core/transaction/transaction_manager.hpp"
#include "core/vindex/vector_index_key.hpp"

#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#ifndef _WIN32
#include <sys/wait.h>
#endif

namespace
{
using namespace litedb::core;

struct StageCase
{
    transaction::CommitStage stage;
    const char * name;
    bool commit_must_be_durable;
};

constexpr StageCase Stages[] {
    {transaction::CommitStage::AfterPrepare, "after_prepare", false},
    {transaction::CommitStage::AfterWalBegin, "after_wal_begin", false},
    {transaction::CommitStage::AfterWalWrites, "after_wal_writes", false},
    {transaction::CommitStage::AfterWalCommitAppend, "after_wal_commit_append", false},
    {transaction::CommitStage::AfterWalCommitFlush, "after_wal_commit_flush", true},
    {transaction::CommitStage::AfterApply, "after_apply", true},
    {transaction::CommitStage::AfterRuntimeReload, "after_runtime_reload", true},
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
    if (!opened) throw std::runtime_error(opened.error().message);
    return std::move(*opened);
}

executor::ExecutionResult execute_ok(database::Session & session, std::string_view sql)
{
    auto result = session.execute_sql(sql);
    if (!result) throw std::runtime_error(result.error().message);
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
    execute_ok(session, "CREATE COLLECTION docs (id BIGINT, embedding VECTOR(2));");
    execute_ok(session, "CREATE INDEX idx_id ON docs (id) USING BTREE;");
    execute_ok(session, "CREATE VINDEX vidx_embedding ON docs (embedding) USING HNSW;");
}

int run_worker(const std::filesystem::path & directory, const StageCase & selected)
{
    transaction::TransactionOptions options {
        .commit_stage_hook = [stage = selected.stage](transaction::CommitStage current, transaction::TransactionId) {
            if (current == stage) {
                std::_Exit(86);
            }
            return false;
        },
    };
    auto engine = open_database(directory, std::move(options));
    database::Session session {*engine};
    execute_ok(session, "USE demo;");
    execute_ok(session, "INSERT INTO docs VALUES (7, [1.0, 0.0]);");
    return 0;
}

void verify_recovered_state(const std::filesystem::path & directory, const StageCase & selected)
{
    auto engine = open_database(directory);
    require(!std::filesystem::exists(directory / ".transactions"), "stale staging directory was not cleaned");
    database::Session session {*engine};
    execute_ok(session, "USE demo;");
    const auto selected_rows = execute_ok(session, "SELECT id FROM docs;");
    require(selected_rows.rows.size() <= 1, "crash recovery exposed a partial or duplicate row state");
    if (selected.commit_must_be_durable) {
        require(selected_rows.rows.size() == 1, "durable commit was not recovered");
    }

    const auto * database_entry = engine->meta().find_database("demo");
    require(database_entry != nullptr, "database metadata missing after crash recovery");
    const auto * collection = engine->meta().find_collection(database_entry->id(), "docs");
    require(collection != nullptr, "collection metadata missing after crash recovery");
    const auto * scalar_entry = engine->meta().find_index(collection->id(), "idx_id");
    const auto * vector_entry = engine->meta().find_vector_index(collection->id(), "vidx_embedding");
    require(scalar_entry != nullptr && vector_entry != nullptr, "index metadata missing after crash recovery");

    auto scalar_key = index::ScalarIndexKey::from_value(common::Value {std::int64_t {7}});
    require(scalar_key.has_value(), "scalar key construction failed");
    auto scalar = engine->index_engine().find_equal(scalar_entry->id(), *scalar_key);
    require(scalar.has_value(), "scalar index lookup failed after crash recovery");

    auto vector_key = vindex::VectorIndexKey::from_vector({1.0, 0.0});
    require(vector_key.has_value(), "vector key construction failed");
    auto vector = engine->vector_index_engine().search(vector_entry->id(), *vector_key, {.top_k = 1});
    require(vector.has_value(), "vector index lookup failed after crash recovery");

    const auto expected = selected_rows.rows.size();
    require(scalar->size() == expected && vector->size() == expected,
            "storage, scalar index, and vector index disagree after crash recovery");
    if (expected == 1) {
        require((*vector)[0].record_id == (*scalar)[0], "recovered indexes reference different records");
    }

    const auto observation = engine->observability();
    require(observation.transaction.wal_size_bytes > 0, "WAL size observation was not published");
    if (expected == 1) {
        require(observation.recovered_committed_transactions >= 1 && observation.replayed_writes >= 1,
                "redo observation was not published");
    }
}

void run_parent(const std::filesystem::path & executable)
{
    for (const auto & stage : Stages) {
        const auto directory = std::filesystem::temp_directory_path() /
                               (std::string("litedb_transaction_crash_") + stage.name);
        initialize_database(directory);
        const auto arguments = executable.string() + "\" --worker \"" + directory.string() + "\" " + stage.name;
#ifdef _WIN32
        const auto command = "\"\"" + arguments + "\"";
#else
        const auto command = "\"" + arguments;
#endif
        const auto exit_code = std::system(command.c_str());
#ifdef _WIN32
        require(exit_code == 86, "crash worker did not exit at the injected commit stage");
#else
        require(WIFEXITED(exit_code) && WEXITSTATUS(exit_code) == 86,
                "crash worker did not exit at the injected commit stage");
#endif
        verify_recovered_state(directory, stage);
    }
}

} // namespace

int main(int argc, char ** argv)
{
    if (argc == 4 && std::string_view {argv[1]} == "--worker") {
        const auto * stage = find_stage(argv[3]);
        require(stage != nullptr, "unknown crash stage");
        return run_worker(argv[2], *stage);
    }
    run_parent(std::filesystem::absolute(argv[0]));
    return 0;
}
