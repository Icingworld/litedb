#include "core/filesystem/platform_filesystem.hpp"
#include "core/meta/meta_engine.hpp"

#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace
{

using namespace litedb::core;

void require(bool condition, const char * message)
{
    if (!condition) throw std::runtime_error(message);
}

meta::CreateCollectionRequest users_request(common::DatabaseId database_id)
{
    return meta::CreateCollectionRequest {
        .database_id = database_id,
        .name = "Users",
        .columns = {
            {.name = "id", .type = {common::LogicalTypeId::BigInt, std::nullopt}, .unique = true, .nullable = false},
            {.name = "name", .type = {common::LogicalTypeId::Varchar, 64}},
            {.name = "embedding", .type = {common::LogicalTypeId::Vector, 3}},
        },
        .comment = "users collection",
    };
}

void test_memory_engine_crud()
{
    meta::MetaEngine engine;
    auto database = engine.create_database({.name = "Main"});
    require(database.has_value(), "create database failed");
    require(engine.find_database("main") != nullptr, "case insensitive database lookup failed");

    auto collection = engine.create_collection(users_request(*database));
    require(collection.has_value(), "create collection failed");
    require(engine.list_columns(*collection).size() == 3, "collection columns mismatch");
    const auto * id = engine.find_column(*collection, "id");
    const auto * name = engine.find_column(*collection, "name");
    const auto * embedding = engine.find_column(*collection, "embedding");
    require(id != nullptr && name != nullptr && embedding != nullptr, "column lookup failed");

    auto scalar_index = engine.create_index({
        .collection_id = *collection,
        .column_ids = {id->id(), name->id()},
        .name = "idx_identity",
        .unique = true,
    });
    require(scalar_index.has_value(), "create composite index failed");

    auto vector_index = engine.create_vector_index({
        .collection_id = *collection,
        .column_id = embedding->id(),
        .name = "vidx_embedding",
        .metric = meta::entry::VectorDistanceMetric::Cosine,
        .hnsw_options = {.max_neighbors = 24, .ef_construction = 240, .ef_search_default = 80, .random_seed = 7},
    });
    require(vector_index.has_value(), "create vector index failed");
    require(engine.find_index(*scalar_index)->column_ids().size() == 2, "composite index columns mismatch");
    require(engine.find_vector_index(*vector_index)->dimension() == 3, "vector index dimension mismatch");

    auto duplicate = engine.create_collection(users_request(*database));
    require(!duplicate.has_value() && duplicate.error().code == meta::MetaEngineErrorCode::DuplicateCollection,
            "duplicate collection error mismatch");

    require(engine.drop_vector_index({.collection_id = *collection, .name = "vidx_embedding"}).has_value(),
            "drop vector index failed");
    require(engine.drop_index({.collection_id = *collection, .name = "idx_identity"}).has_value(),
            "drop index failed");
    require(engine.drop_collection({.database_id = *database, .name = "users"}).has_value(),
            "drop collection failed");
    require(engine.drop_database({.name = "main"}).has_value(), "drop database failed");
    require(engine.list_databases().empty(), "database remained after drop");
}

void test_persistent_engine_roundtrip(const std::filesystem::path & path)
{
    auto filesystem = filesystem::create_platform_filesystem();
    meta::MetaStore store {path, filesystem};
    common::DatabaseId database_id = 0;
    common::CollectionId collection_id = 0;
    {
        meta::MetaEngine engine {store};
        require(engine.load().has_value(), "initial meta engine load failed");
        auto database = engine.create_database({.name = "main"});
        require(database.has_value(), "persistent create database failed");
        database_id = *database;
        auto collection = engine.create_collection(users_request(database_id));
        require(collection.has_value(), "persistent create collection failed");
        collection_id = *collection;
    }
    {
        meta::MetaEngine reopened {store};
        require(reopened.load().has_value(), "reopen meta engine failed");
        require(reopened.find_database(database_id) != nullptr, "reopened database missing");
        require(reopened.find_collection(collection_id) != nullptr, "reopened collection missing");
        require(reopened.list_columns(collection_id).size() == 3, "reopened columns mismatch");
    }
}

void test_store_failure_rolls_back(const std::filesystem::path & directory)
{
    std::filesystem::create_directories(directory);
    const auto regular_file = directory / "not_a_directory";
    std::ofstream output {regular_file};
    output << "x";
    output.close();

    auto filesystem = filesystem::create_platform_filesystem();
    meta::MetaStore store {regular_file / "meta.ldb", filesystem};
    meta::MetaEngine engine {store};
    auto created = engine.create_database({.name = "must_rollback"});
    require(!created.has_value(), "create should fail when meta store cannot create its directory");
    require(created.error().code == meta::MetaEngineErrorCode::StoreError, "store error code mismatch");
    require(created.error().store_code == meta::MetaStoreErrorCode::FileSystemError, "nested store code mismatch");
    require(engine.find_database("must_rollback") == nullptr, "failed mutation was not rolled back");
}

void test_invalid_snapshot_rejected()
{
    meta::MetaEngine engine;
    meta::MetaSnapshot snapshot;
    snapshot.next_database_id = 1;
    snapshot.databases.push_back({1, "main", {}});
    auto restored = engine.restore(snapshot);
    require(!restored.has_value(), "invalid next id should reject snapshot");
    require(restored.error().code == meta::MetaEngineErrorCode::InvalidSnapshot,
            "invalid snapshot error code mismatch");
}

} // namespace

int main()
{
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto directory = std::filesystem::temp_directory_path() / ("litedb_meta_engine_" + std::to_string(suffix));
    try {
        test_memory_engine_crud();
        test_persistent_engine_roundtrip(directory / "meta.ldb");
        test_store_failure_rolls_back(directory / "rollback");
        test_invalid_snapshot_rejected();
        std::filesystem::remove_all(directory);
    } catch (const std::exception & exception) {
        std::filesystem::remove_all(directory);
        std::cerr << exception.what() << '\n';
        return 1;
    }
    return 0;
}
