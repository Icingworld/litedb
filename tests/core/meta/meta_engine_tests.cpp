#include "core/filesystem/platform_filesystem.hpp"
#include "core/meta/meta_engine.hpp"

#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace
{

using namespace litedb::core;

template <typename T>
concept CanCreateDatabase = requires(T & value) {
    value.create_database(meta::CreateDatabaseRequest {.name = "x"});
};

template <typename T>
concept CanPublish = requires(T & value, const meta::MetaSnapshot & snapshot) {
    value.publish_committed(snapshot);
};

static_assert(!CanCreateDatabase<meta::CatalogView>);
static_assert(CanCreateDatabase<meta::CatalogEditor>);
static_assert(!CanPublish<meta::CatalogEditor>);
static_assert(!CanCreateDatabase<meta::CatalogPublisher>);

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
    meta::CatalogEditor engine;
    auto database = engine.create_database({.name = "Main"});
    require(database.has_value(), "create database failed");
    require(engine.view().find_database("main") != nullptr, "case insensitive database lookup failed");

    auto collection = engine.create_collection(users_request(*database));
    require(collection.has_value(), "create collection failed");
    require(engine.view().list_columns(*collection).size() == 3, "collection columns mismatch");
    const auto * id = engine.view().find_column(*collection, "id");
    const auto * name = engine.view().find_column(*collection, "name");
    const auto * embedding = engine.view().find_column(*collection, "embedding");
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
    require(engine.view().find_index(*scalar_index)->column_ids().size() == 2, "composite index columns mismatch");
    require(engine.view().find_vector_index(*vector_index)->dimension() == 3, "vector index dimension mismatch");

    auto duplicate = engine.create_collection(users_request(*database));
    require(!duplicate.has_value() && duplicate.error().is(meta::MetaErrorCode::DuplicateCollection),
            "duplicate collection error mismatch");

    require(engine.drop_vector_index({.collection_id = *collection, .name = "vidx_embedding"}).has_value(),
            "drop vector index failed");
    require(engine.drop_index({.collection_id = *collection, .name = "idx_identity"}).has_value(),
            "drop index failed");
    require(engine.drop_collection({.database_id = *database, .name = "users"}).has_value(),
            "drop collection failed");
    require(engine.drop_database({.name = "main"}).has_value(), "drop database failed");
    require(engine.view().list_databases().empty(), "database remained after drop");
}

void test_persistent_engine_roundtrip(const std::filesystem::path & path)
{
    auto filesystem = filesystem::create_platform_filesystem();
    common::DatabaseId database_id = 0;
    common::CollectionId collection_id = 0;
    {
        meta::CatalogPublisher publisher {path, filesystem};
        require(publisher.open_or_initialize().has_value(), "initial catalog open failed");
        auto editor_result = meta::CatalogEditor::from(publisher.view());
        require(editor_result.has_value(), "create editor failed");
        auto engine = std::move(*editor_result);
        auto database = engine.create_database({.name = "main"});
        require(database.has_value(), "persistent create database failed");
        database_id = *database;
        auto collection = engine.create_collection(users_request(database_id));
        require(collection.has_value(), "persistent create collection failed");
        collection_id = *collection;
        meta::MetaStore store {path, filesystem};
        require(store.save(engine.snapshot()).has_value(), "save committed snapshot failed");
        require(publisher.publish_committed(engine.snapshot()).has_value(), "publish committed snapshot failed");
    }
    {
        meta::CatalogPublisher reopened {path, filesystem};
        require(reopened.open_or_initialize().has_value(), "reopen catalog failed");
        require(reopened.view().find_database(database_id) != nullptr, "reopened database missing");
        require(reopened.view().find_collection(collection_id) != nullptr, "reopened collection missing");
        require(reopened.view().list_columns(collection_id).size() == 3, "reopened columns mismatch");
    }
}

void test_editor_has_no_implicit_io(const std::filesystem::path & directory)
{
    std::filesystem::create_directories(directory);
    const auto regular_file = directory / "not_a_directory";
    std::ofstream output {regular_file};
    output << "x";
    output.close();

    auto filesystem = filesystem::create_platform_filesystem();
    meta::CatalogEditor engine;
    auto created = engine.create_database({.name = "must_rollback"});
    require(created.has_value(), "offline editor must not perform file IO");
    require(engine.view().find_database("must_rollback") != nullptr, "offline mutation was not retained");
}

void test_invalid_snapshot_rejected()
{
    meta::MetaSnapshot snapshot;
    snapshot.next_database_id = 1;
    snapshot.databases.push_back({1, "main", {}});
    auto restored = meta::CatalogEditor::from(snapshot);
    require(!restored.has_value(), "invalid next id should reject snapshot");
    require(restored.error().is(meta::MetaErrorCode::InvalidSnapshot),
            "invalid snapshot error code mismatch");
}

void test_empty_collection_snapshot_rejected()
{
    meta::MetaSnapshot snapshot;
    snapshot.next_database_id = 2;
    snapshot.next_collection_id = 2;
    snapshot.databases.push_back({1, "main", {{1, 1, "empty", std::nullopt, {}, {}, {}}}});
    auto restored = meta::CatalogEditor::from(snapshot);
    require(!restored.has_value(), "empty collection snapshot should be rejected");
    require(restored.error().is(meta::MetaErrorCode::InvalidSnapshot),
            "empty collection snapshot error code mismatch");
}

void test_id_exhaustion_rejected()
{
    meta::MetaSnapshot snapshot;
    snapshot.next_database_id = std::numeric_limits<common::DatabaseId>::max();
    auto editor = meta::CatalogEditor::from(snapshot);
    require(editor.has_value(), "maximum next id should be a structurally valid snapshot");
    auto created = editor->create_database({.name = "overflow"});
    require(!created.has_value() && created.error().is(meta::MetaErrorCode::InvalidState),
            "database id exhaustion should be rejected before allocation");
}

} // namespace

int main()
{
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto directory = std::filesystem::temp_directory_path() / ("litedb_meta_engine_" + std::to_string(suffix));
    try {
        test_memory_engine_crud();
        test_persistent_engine_roundtrip(directory / "meta.ldb");
        test_editor_has_no_implicit_io(directory / "rollback");
        test_invalid_snapshot_rejected();
        test_empty_collection_snapshot_rejected();
        test_id_exhaustion_rejected();
        std::filesystem::remove_all(directory);
    } catch (const std::exception & exception) {
        std::filesystem::remove_all(directory);
        std::cerr << exception.what() << '\n';
        return 1;
    }
    return 0;
}
