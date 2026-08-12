#include "core/filesystem/platform_filesystem.hpp"
#include "core/catalog/catalog_editor.hpp"
#include "core/catalog/catalog_publisher.hpp"
#include "core/catalog/catalog_store.hpp"

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
    value.create_database(catalog::CreateDatabaseRequest {.database_name = "x"});
};

template <typename T>
concept CanPublish = requires(T & value, const catalog::CatalogSnapshot & snapshot) {
    value.publish_committed(snapshot);
};

static_assert(!CanCreateDatabase<catalog::CatalogViewer>);
static_assert(CanCreateDatabase<catalog::CatalogEditor>);
static_assert(!CanPublish<catalog::CatalogEditor>);
static_assert(!CanCreateDatabase<catalog::CatalogPublisher>);

void require(bool condition, const char * message)
{
    if (!condition) throw std::runtime_error(message);
}

catalog::CreateCollectionRequest users_request(common::DatabaseId database_id)
{
    return catalog::CreateCollectionRequest {
        .database_id = database_id,
        .collection_name = "Users",
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
    catalog::CatalogEditor engine;
    auto database = engine.create_database({.database_name = "Main"});
    require(database.has_value(), "create database failed");
    require(engine.view().find_database("main").has_value(), "case insensitive database lookup failed");

    auto collection = engine.create_collection(users_request(*database));
    require(collection.has_value(), "create collection failed");
    require(engine.view().list_columns(*collection).size() == 3, "collection columns mismatch");
    const auto id = engine.view().find_column(*collection, "id");
    const auto name = engine.view().find_column(*collection, "name");
    const auto embedding = engine.view().find_column(*collection, "embedding");
    require(id && name && embedding, "column lookup failed");
    const auto id_column_id = id->id();
    const auto name_column_id = name->id();
    const auto embedding_column_id = embedding->id();
    const auto implicit_indexes = engine.view().list_indexes(*collection);
    require(implicit_indexes.size() == 1, "UNIQUE column should create one implicit index");
    require(implicit_indexes.front().get().unique(), "implicit UNIQUE index should be unique");
    require(implicit_indexes.front().get().column_id() == id_column_id, "implicit UNIQUE index column mismatch");
    const auto implicit_index_id = implicit_indexes.front().get().id();

    auto scalar_index = engine.create_index({
        .collection_id = *collection,
        .column_id = name_column_id,
        .index_name = "idx_name",
    });
    require(scalar_index.has_value(), "create scalar index failed");

    auto vector_index = engine.create_vector_index({
        .collection_id = *collection,
        .column_id = embedding_column_id,
        .vector_index_name = "vidx_embedding",
        .metric = catalog::entry::VectorDistanceMetric::Cosine,
        .hnsw_options = {.max_neighbors = 24, .ef_construction = 240, .ef_search_default = 80, .random_seed = 7},
    });
    require(vector_index.has_value(), "create vector index failed");
    require(engine.view().find_index(*scalar_index)->column_id() == name_column_id, "scalar index column mismatch");
    require(engine.view().find_vector_index(*vector_index)->dimension() == 3, "vector index dimension mismatch");

    auto duplicate = engine.create_collection(users_request(*database));
    require(!duplicate.has_value() && duplicate.error().is(catalog::CatalogErrorCode::DuplicateCollection),
            "duplicate collection error mismatch");

    require(engine.drop_vector_index({.vector_index_id = *vector_index}).has_value(),
            "drop vector index failed");
    auto implicit_drop = engine.drop_index({.index_id = implicit_index_id});
    require(!implicit_drop.has_value()
                && implicit_drop.error().is(catalog::CatalogErrorCode::InvalidArgument),
            "implicit UNIQUE index should not be droppable");
    require(engine.drop_index({.index_id = *scalar_index}).has_value(),
            "drop index failed");
    require(engine.drop_collection({.collection_id = *collection}).has_value(),
            "drop collection failed");
    require(engine.drop_database({.database_id = *database}).has_value(), "drop database failed");
    require(engine.view().list_databases().empty(), "database remained after drop");
}

void test_drop_requests_validate_current_state_and_cascade()
{
    catalog::CatalogEditor engine;

    const auto missing_database = engine.drop_database({.database_id = 99});
    require(!missing_database && missing_database.error().is(catalog::CatalogErrorCode::DatabaseNotFound),
            "missing database drop error mismatch");
    const auto missing_collection = engine.drop_collection({.collection_id = 99});
    require(!missing_collection && missing_collection.error().is(catalog::CatalogErrorCode::CollectionNotFound),
            "missing collection drop error mismatch");
    const auto missing_index = engine.drop_index({.index_id = 99});
    require(!missing_index && missing_index.error().is(catalog::CatalogErrorCode::IndexNotFound),
            "missing index drop error mismatch");
    const auto missing_vector_index = engine.drop_vector_index({.vector_index_id = 99});
    require(!missing_vector_index
                && missing_vector_index.error().is(catalog::CatalogErrorCode::VectorIndexNotFound),
            "missing vector index drop error mismatch");

    const auto database = engine.create_database({.database_name = "main"});
    require(database.has_value(), "cascade database creation failed");
    const auto collection = engine.create_collection(users_request(*database));
    require(collection.has_value(), "cascade collection creation failed");
    const auto columns = engine.view().list_columns(*collection);
    const auto indexes = engine.view().list_indexes(*collection);
    require(columns.size() == 3 && indexes.size() == 1, "cascade fixture mismatch");
    const auto first_column_id = columns.front().get().id();
    const auto implicit_index_id = indexes.front().get().id();

    require(engine.drop_collection({.collection_id = *collection}).has_value(),
            "validated collection drop failed");
    require(!engine.view().find_collection(*collection),
            "collection remained after drop");
    require(!engine.view().find_column(first_column_id),
            "collection column remained after drop");
    require(!engine.view().find_index(implicit_index_id),
            "collection index remained after drop");
    require(engine.view().list_collections(*database).empty(),
            "database collection index remained after drop");
    const auto stale_collection = engine.drop_collection({.collection_id = *collection});
    require(!stale_collection && stale_collection.error().is(catalog::CatalogErrorCode::CollectionNotFound),
            "stale collection id should be rejected");

    const auto replacement = engine.create_collection(users_request(*database));
    require(replacement.has_value(), "replacement collection creation failed");
    const auto replacement_columns = engine.view().list_columns(*replacement);
    const auto replacement_indexes = engine.view().list_indexes(*replacement);
    require(replacement_columns.size() == 3 && replacement_indexes.size() == 1,
            "replacement collection fixture mismatch");
    const auto replacement_column_id = replacement_columns.front().get().id();
    const auto replacement_index_id = replacement_indexes.front().get().id();

    require(engine.drop_database({.database_id = *database}).has_value(),
            "validated database drop failed");
    require(!engine.view().find_database(*database),
            "database remained after cascade drop");
    require(!engine.view().find_collection(*replacement),
            "database collection remained after cascade drop");
    require(!engine.view().find_column(replacement_column_id),
            "database column remained after cascade drop");
    require(!engine.view().find_index(replacement_index_id),
            "database index remained after cascade drop");
}

void test_persistent_engine_roundtrip(const std::filesystem::path & path)
{
    auto filesystem = filesystem::create_platform_filesystem();
    common::DatabaseId database_id = 0;
    common::CollectionId collection_id = 0;
    {
        catalog::CatalogPublisher publisher {path, filesystem};
        require(publisher.open_or_initialize().has_value(), "initial catalog open failed");
        auto editor_result = catalog::CatalogEditor::from(publisher.view());
        require(editor_result.has_value(), "create editor failed");
        auto engine = std::move(*editor_result);
        auto database = engine.create_database({.database_name = "main"});
        require(database.has_value(), "persistent create database failed");
        database_id = *database;
        auto collection = engine.create_collection(users_request(database_id));
        require(collection.has_value(), "persistent create collection failed");
        collection_id = *collection;
        catalog::CatalogStore store {path, filesystem};
        require(store.save(engine.snapshot()).has_value(), "save committed snapshot failed");
        require(publisher.publish_committed(engine.snapshot()).has_value(), "publish committed snapshot failed");
    }
    {
        catalog::CatalogPublisher reopened {path, filesystem};
        require(reopened.open_or_initialize().has_value(), "reopen catalog failed");
        require(reopened.view().find_database(database_id).has_value(), "reopened database missing");
        require(reopened.view().find_collection(collection_id).has_value(), "reopened collection missing");
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
    catalog::CatalogEditor engine;
    auto created = engine.create_database({.database_name = "must_rollback"});
    require(created.has_value(), "offline editor must not perform file IO");
    require(engine.view().find_database("must_rollback").has_value(), "offline mutation was not retained");
}

void test_invalid_snapshot_rejected()
{
    catalog::CatalogSnapshot snapshot;
    snapshot.next_database_id = 1;
    snapshot.databases.push_back({1, "main", {}});
    auto restored = catalog::CatalogEditor::from(snapshot);
    require(!restored.has_value(), "invalid next id should reject snapshot");
    require(restored.error().is(catalog::CatalogErrorCode::InvalidSnapshot),
            "invalid snapshot error code mismatch");
}

void test_empty_collection_snapshot_rejected()
{
    catalog::CatalogSnapshot snapshot;
    snapshot.next_database_id = 2;
    snapshot.next_collection_id = 2;
    snapshot.databases.push_back({1, "main", {{1, 1, "empty", std::nullopt, {}, {}, {}}}});
    auto restored = catalog::CatalogEditor::from(snapshot);
    require(!restored.has_value(), "empty collection snapshot should be rejected");
    require(restored.error().is(catalog::CatalogErrorCode::InvalidSnapshot),
            "empty collection snapshot error code mismatch");
}

void test_id_exhaustion_rejected()
{
    catalog::CatalogSnapshot snapshot;
    snapshot.next_database_id = std::numeric_limits<common::DatabaseId>::max();
    auto editor = catalog::CatalogEditor::from(snapshot);
    require(editor.has_value(), "maximum next id should be a structurally valid snapshot");
    auto created = editor->create_database({.database_name = "overflow"});
    require(!created.has_value() && created.error().is(catalog::CatalogErrorCode::InvalidState),
            "database id exhaustion should be rejected before allocation");
}

} // namespace

int main()
{
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto directory = std::filesystem::temp_directory_path() / ("litedb_catalog_engine_" + std::to_string(suffix));
    try {
        test_memory_engine_crud();
        test_drop_requests_validate_current_state_and_cascade();
        test_persistent_engine_roundtrip(directory / "catalog.lcat");
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
