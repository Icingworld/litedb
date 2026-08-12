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
    {transaction::CommitStage::AfterDeltaApply, "after_delta_apply", true},
    {transaction::CommitStage::AfterTruncate, "after_truncate", true},
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

void initialize_database(const std::filesystem::path & directory, std::string_view operation)
{
    std::filesystem::remove_all(directory);
    auto engine = open_database(directory);
    database::Session session {*engine};
    execute_ok(session, "CREATE DATABASE demo;");
    execute_ok(session, "USE demo;");
    if (operation == "create") return;
    execute_ok(session, "CREATE COLLECTION docs (id BIGINT, embedding VECTOR(2));");
    execute_ok(session, "INSERT INTO docs VALUES (7, [1.0, 0.0]);");
    if (operation == "drop") {
        execute_ok(session, "CREATE INDEX idx_id ON docs (id) USING BTREE;");
        execute_ok(session, "CREATE VINDEX vidx_embedding ON docs (embedding) USING HNSW;");
    }
}

int run_worker(
    const std::filesystem::path & directory,
    const StageCase & selected,
    std::string_view operation
)
{
    transaction::TransactionOptions options {
        .commit_stage_hook = [stage = selected.stage](transaction::CommitStage current, transaction::TransactionId) {
            if (current == stage) std::_Exit(87);
            return false;
        },
    };
    auto engine = open_database(directory, std::move(options));
    database::Session session {*engine};
    execute_ok(session, "USE demo;");
    if (operation == "create") {
        execute_ok(session, "CREATE COLLECTION docs (id BIGINT, embedding VECTOR(2));");
    } else if (operation == "drop") {
        execute_ok(session, "DROP COLLECTION docs;");
    } else if (operation == "index") {
        execute_ok(session, "CREATE INDEX idx_id ON docs (id) USING BTREE;");
    } else {
        execute_ok(session, "CREATE VINDEX vidx_embedding ON docs (embedding) USING HNSW;");
    }
    return 0;
}

bool collection_exists(database::DatabaseEngine & engine)
{
    const auto database = engine.catalog().find_database("demo");
    return database.has_value()
        && engine.catalog().find_collection(database->id(), "docs").has_value();
}

void verify_create(const std::filesystem::path & directory, const StageCase & stage)
{
    auto engine = open_database(directory);
    const auto exists = collection_exists(*engine);
    if (stage.commit_must_be_durable) require(exists, "durable CREATE COLLECTION was not recovered");
    require(std::filesystem::exists(directory / "collections" / "1.store") == exists,
            "CREATE COLLECTION catalog and storage file disagree after recovery");
}

void verify_drop(const std::filesystem::path & directory, const StageCase & stage)
{
    auto engine = open_database(directory);
    const auto exists = collection_exists(*engine);
    if (stage.commit_must_be_durable) require(!exists, "durable DROP COLLECTION was not recovered");
    const auto collection_exists_on_disk = std::filesystem::exists(directory / "collections" / "1.store");
    const auto scalar_exists_on_disk = std::filesystem::exists(directory / "indexes" / "1.bti");
    const auto vector_exists_on_disk = std::filesystem::exists(directory / "vindexes" / "vindex_1.lhnsw");
    require(collection_exists_on_disk == exists && scalar_exists_on_disk == exists && vector_exists_on_disk == exists,
            "DROP COLLECTION catalog and participant files disagree after recovery");
    if (exists) {
        database::Session session {*engine};
        execute_ok(session, "USE demo;");
        auto rows = execute_ok(session, "SELECT id FROM docs;");
        require(rows.rows.size() == 1, "pre-commit DROP recovery lost collection rows");
    }
}

void verify_index(const std::filesystem::path & directory, const StageCase & stage)
{
    auto engine = open_database(directory);
    const auto database = engine->catalog().find_database("demo");
    require(database.has_value(), "database missing while verifying CREATE INDEX");
    const auto collection = engine->catalog().find_collection(database->id(), "docs");
    require(collection.has_value(), "collection missing while verifying CREATE INDEX");
    const auto entry = engine->catalog().find_index(collection->id(), "idx_id");
    const auto exists = entry.has_value();
    if (stage.commit_must_be_durable) require(exists, "durable CREATE INDEX was not recovered");
    require(std::filesystem::exists(directory / "indexes" / "1.bti") == exists,
            "CREATE INDEX catalog and file disagree after recovery");
    if (exists) {
        auto key = index::ScalarIndexKey::from_value(common::Value {std::int64_t {7}});
        require(key.has_value(), "scalar key construction failed");
        auto rows = engine->index_engine().find_equal(entry->id(), *key);
        require(rows && rows->size() == 1, "recovered B+Tree does not contain existing row");
    }
}

void verify_vindex(const std::filesystem::path & directory, const StageCase & stage)
{
    auto engine = open_database(directory);
    const auto database = engine->catalog().find_database("demo");
    require(database.has_value(), "database missing while verifying CREATE VINDEX");
    const auto collection = engine->catalog().find_collection(database->id(), "docs");
    require(collection.has_value(), "collection missing while verifying CREATE VINDEX");
    const auto entry = engine->catalog().find_vector_index(collection->id(), "vidx_embedding");
    const auto exists = entry.has_value();
    if (stage.commit_must_be_durable) require(exists, "durable CREATE VINDEX was not recovered");
    require(std::filesystem::exists(directory / "vindexes" / "vindex_1.lhnsw") == exists,
            "CREATE VINDEX catalog and file disagree after recovery");
    if (exists) {
        auto key = vindex::VectorIndexKey::from_vector({1.0, 0.0});
        require(key.has_value(), "vector key construction failed");
        auto rows = engine->vector_index_engine().search(entry->id(), *key, {.top_k = 1});
        require(rows && rows->size() == 1, "recovered HNSW does not contain existing row");
    }
}

int launch_worker(
    const std::filesystem::path & executable,
    const std::filesystem::path & directory,
    const StageCase & stage,
    std::string_view operation
)
{
    const auto arguments = executable.string() + "\" --worker \"" + directory.string() + "\" " +
                           stage.name + " " + std::string(operation);
#ifdef _WIN32
    const auto command = "\"\"" + arguments + "\"";
    return std::system(command.c_str());
#else
    const auto command = "\"" + arguments;
    const auto status = std::system(command.c_str());
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
}

void run_parent(const std::filesystem::path & executable)
{
    for (const auto operation : {
             std::string_view {"create"}, std::string_view {"index"},
             std::string_view {"vindex"}, std::string_view {"drop"}
         }) {
        for (const auto & stage : Stages) {
            const auto directory = std::filesystem::temp_directory_path() /
                                   ("litedb_ddl_crash_" + std::string(operation) + "_" + stage.name);
            initialize_database(directory, operation);
            require(launch_worker(executable, directory, stage, operation) == 87,
                    "DDL crash worker did not exit at the injected stage");
            if (operation == "create") verify_create(directory, stage);
            else if (operation == "index") verify_index(directory, stage);
            else if (operation == "vindex") verify_vindex(directory, stage);
            else verify_drop(directory, stage);
        }
    }
}

} // namespace

int main(int argc, char ** argv)
{
    if (argc == 5 && std::string_view {argv[1]} == "--worker") {
        const auto * stage = find_stage(argv[3]);
        require(stage != nullptr, "unknown DDL crash stage");
        return run_worker(argv[2], *stage, argv[4]);
    }
    run_parent(std::filesystem::absolute(argv[0]));
    return 0;
}
