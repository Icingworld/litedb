#include "core/vindex/flat_index/flat_index.hpp"
#include "core/vindex/hnsw_index/hnsw_index.hpp"
#include "core/vindex/vector_index_key.hpp"
#include "core/vindex/vector_index_engine.hpp"

#include <chrono>
#include <array>
#include <cmath>
#include <exception>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "core/common/logical_type.hpp"
#include "core/filesystem/platform_filesystem.hpp"
#include "core/meta/meta_engine.hpp"
#include "core/schema/collection.hpp"
#include "core/storage/schema_loader.hpp"
#include "core/storage/storage_engine.hpp"

namespace
{

using namespace litedb::core;
using namespace litedb::core::common;
using namespace litedb::core::schema;
using namespace litedb::core::vindex;

void require(bool condition, const char * message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::filesystem::path temporary_path()
{
    return std::filesystem::temp_directory_path()
        / ("litedb-flat-index-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()
        ));
}

CollectionSchema vectors_schema()
{
    return CollectionSchema {
        1,
        20,
        "vectors",
        {
            ColumnSchema {30, 20, 0, "name", {LogicalTypeId::Varchar, 64}, false, false, {}, {}},
            ColumnSchema {31, 20, 1, "embedding", {LogicalTypeId::Vector, 2}, true, false, {}, {}},
        },
    };
}

RecordData vector_record(std::string name, Value embedding)
{
    return RecordData {{Value {std::move(name)}, std::move(embedding)}};
}

void require_records(
    const std::vector<VectorSearchResult> & actual,
    std::vector<RecordId> expected,
    const char * message
)
{
    if (actual.size() != expected.size()) {
        throw std::runtime_error(message);
    }
    for (std::size_t index = 0; index < expected.size(); ++index) {
        if (actual[index].record_id != expected[index]) {
            throw std::runtime_error(message);
        }
    }
}

std::vector<VectorSearchResult> results(std::expected<std::vector<VectorSearchResult>, VectorIndexError> result)
{
    if (!result.has_value()) {
        throw std::runtime_error(result.error().message);
    }
    return std::move(result.value());
}

VectorIndexKey vector_key(VectorValue vector)
{
    auto result = VectorIndexKey::from_vector(std::move(vector));
    if (!result.has_value()) {
        throw std::runtime_error(result.error().message);
    }
    return std::move(result.value());
}

HnswIndexOptions hnsw_options()
{
    return HnswIndexOptions {
        .dimension = 2,
        .metric = VectorDistanceMetric::L2,
        .max_neighbors = 4,
        .ef_construction = 32,
        .ef_search_default = 32,
        .random_seed = 7,
    };
}

void test_hnsw_index_persists_search_and_tombstones()
{
    auto filesystem = filesystem::create_platform_filesystem();
    const auto directory = temporary_path();
    const auto index_path = directory / "index.lhnsw";
    {
        auto created = HnswIndex::create(index_path, 10, 20, 31, hnsw_options(), filesystem);
        require(created.has_value(), "create hnsw index failed");
        auto & index = *created;
        require(index.insert(vector_key({0.0, 0.0}), 1).has_value(), "insert first hnsw vector failed");
        require(index.insert(vector_key({1.0, 0.0}), 2).has_value(), "insert second hnsw vector failed");
        require(index.insert(vector_key({5.0, 0.0}), 3).has_value(), "insert third hnsw vector failed");
        require(index.size() == 3, "hnsw size mismatch");
        require_records(
            results(index.search(vector_key({0.2, 0.0}), VectorSearchRequest {.top_k = 2})),
            {1, 2},
            "hnsw nearest vector order mismatch"
        );

        require(index.erase(1).has_value(), "erase hnsw vector failed");
        require(index.insert(vector_key({10.0, 0.0}), 1).has_value(), "reinsert hnsw record failed");
        require(index.size() == 3, "hnsw active size after reinsert mismatch");
        require_records(
            results(index.search(vector_key({0.2, 0.0}), VectorSearchRequest {.top_k = 2})),
            {2, 3},
            "hnsw tombstone should not be returned"
        );
    }
    {
        auto opened = HnswIndex::open(index_path, 10, 20, 31, hnsw_options(), filesystem);
        require(opened.has_value(), "open persisted hnsw index failed");
        require(opened->size() == 3, "reopened hnsw size mismatch");
        require_records(
            results(opened->search(vector_key({0.2, 0.0}), VectorSearchRequest {.top_k = 2})),
            {2, 3},
            "reopened hnsw search mismatch"
        );
    }
    std::filesystem::remove_all(directory);
}

void test_hnsw_store_recovers_truncated_tail()
{
    auto filesystem = filesystem::create_platform_filesystem();
    const auto directory = temporary_path();
    const auto index_path = directory / "truncated.lhnsw";
    {
        auto created = HnswIndex::create(index_path, 11, 20, 31, hnsw_options(), filesystem);
        require(created.has_value(), "create truncation hnsw index failed");
        require(created->insert(vector_key({0.0, 0.0}), 1).has_value(), "insert truncation hnsw vector failed");
    }
    const auto committed_size = std::filesystem::file_size(index_path);
    {
        auto file = filesystem.open(index_path, {
            .access = filesystem::FileAccess::ReadWrite,
            .create_mode = filesystem::FileCreateMode::OpenExisting,
        });
        require(file.has_value(), "open hnsw file for tail append failed");
        const std::array garbage {std::byte {0x48}, std::byte {0x57}, std::byte {0x43}};
        require(file->append(garbage).has_value(), "append truncated hnsw tail failed");
        require(file->sync_data().has_value(), "sync truncated hnsw tail failed");
    }
    auto opened = HnswIndex::open(index_path, 11, 20, 31, hnsw_options(), filesystem);
    require(opened.has_value(), "hnsw store should recover a truncated tail");
    require(opened->size() == 1, "truncated hnsw recovery lost committed data");
    require(std::filesystem::file_size(index_path) == committed_size, "truncated hnsw tail was not removed");
    std::filesystem::remove_all(directory);
}

void test_hnsw_store_rejects_corrupted_commit()
{
    auto filesystem = filesystem::create_platform_filesystem();
    const auto directory = temporary_path();
    const auto index_path = directory / "corrupted.lhnsw";
    {
        auto created = HnswIndex::create(index_path, 12, 20, 31, hnsw_options(), filesystem);
        require(created.has_value(), "create corruption hnsw index failed");
        require(created->insert(vector_key({0.0, 0.0}), 1).has_value(), "insert corruption hnsw vector failed");
    }
    {
        auto file = filesystem.open(index_path, {
            .access = filesystem::FileAccess::ReadWrite,
            .create_mode = filesystem::FileCreateMode::OpenExisting,
        });
        require(file.has_value(), "open hnsw file for corruption failed");
        const std::array corrupted {std::byte {0xff}};
        require(file->write_at(
            hnsw_index::HnswStoreCodec::HeaderSize + hnsw_index::HnswStoreCodec::FramePrefixSize,
            corrupted
        ).has_value(), "corrupt hnsw commit failed");
        require(file->sync_data().has_value(), "sync corrupted hnsw commit failed");
    }
    auto opened = HnswIndex::open(index_path, 12, 20, 31, hnsw_options(), filesystem);
    require(!opened.has_value(), "corrupted hnsw commit should be rejected");
    require(opened.error().code == VectorIndexErrorCode::CorruptedIndex, "corrupted hnsw error code mismatch");
    std::filesystem::remove_all(directory);
}

void test_hnsw_rejects_descriptor_mismatch()
{
    auto filesystem = filesystem::create_platform_filesystem();
    const auto directory = temporary_path();
    const auto index_path = directory / "descriptor.lhnsw";
    {
        auto created = HnswIndex::create(index_path, 13, 20, 31, hnsw_options(), filesystem);
        require(created.has_value(), "create descriptor hnsw index failed");
        require(created->insert(vector_key({0.0, 0.0}), 1).has_value(), "insert descriptor vector failed");
    }
    auto mismatched = hnsw_options();
    mismatched.metric = VectorDistanceMetric::Cosine;
    auto opened = HnswIndex::open(index_path, 13, 20, 31, mismatched, filesystem);
    require(!opened.has_value(), "HNSW descriptor mismatch should be rejected");
    require(opened.error().code == VectorIndexErrorCode::CorruptedIndex, "descriptor mismatch error code mismatch");
    std::filesystem::remove_all(directory);
}

void test_hnsw_matches_brute_force_top_one()
{
    auto filesystem = filesystem::create_platform_filesystem();
    const auto directory = temporary_path();
    const auto index_path = directory / "recall.lhnsw";
    auto options = hnsw_options();
    options.max_neighbors = 8;
    options.ef_construction = 64;
    options.ef_search_default = 32;
    auto created = HnswIndex::create(index_path, 14, 20, 31, options, filesystem);
    require(created.has_value(), "create recall hnsw index failed");

    std::vector<common::VectorValue> vectors;
    vectors.reserve(128);
    for (std::size_t index = 0; index < 128; ++index) {
        const auto x = std::sin(static_cast<double>(index) * 0.37) * 10.0;
        const auto y = std::cos(static_cast<double>(index) * 0.61) * 10.0;
        vectors.push_back({x, y});
        require(created->insert(vector_key({x, y}), index + 1).has_value(), "insert recall vector failed");
    }

    std::size_t matches = 0;
    for (std::size_t query_index = 0; query_index < 32; ++query_index) {
        const common::VectorValue query {
            std::sin(static_cast<double>(query_index) * 0.43) * 9.0,
            std::cos(static_cast<double>(query_index) * 0.29) * 9.0,
        };
        std::size_t expected = 0;
        double best = std::numeric_limits<double>::infinity();
        for (std::size_t index = 0; index < vectors.size(); ++index) {
            const auto dx = query[0] - vectors[index][0];
            const auto dy = query[1] - vectors[index][1];
            const auto distance = dx * dx + dy * dy;
            if (distance < best) {
                best = distance;
                expected = index + 1;
            }
        }
        auto nearest = created->search(vector_key(query), {.top_k = 1});
        require(nearest.has_value() && nearest->size() == 1, "recall HNSW search failed");
        matches += nearest->front().record_id == expected ? 1 : 0;
    }
    require(matches >= 30, "HNSW top-one recall is below the expected threshold");
    std::filesystem::remove_all(directory);
}

void test_flat_index_scans_storage()
{
    auto filesystem = filesystem::create_platform_filesystem();
    const auto path = temporary_path();
    {
        storage::StorageEngine storage {path, filesystem};
        require(storage.create_collection(vectors_schema()).has_value(), "create vector collection failed");
        require(storage.insert(20, vector_record("first", Value {VectorValue {0.0, 0.0}})).has_value(), "insert first vector failed");
        require(storage.insert(20, vector_record("second", Value {VectorValue {1.0, 0.0}})).has_value(), "insert second vector failed");
        require(storage.insert(20, vector_record("third", Value {VectorValue {5.0, 0.0}})).has_value(), "insert third vector failed");
        require(storage.insert(20, vector_record("null", Value::null())).has_value(), "insert null vector failed");

        FlatIndex index(FlatIndexOptions {
            .collection_id = 20,
            .column_ordinal = 1,
            .dimension = 2,
            .metric = VectorDistanceMetric::L2,
        }, storage);

        require(index.kind() == VectorIndexKind::Flat, "flat kind mismatch");
        require(index.dimension() == 2, "flat dimension mismatch");
        require(index.metric() == VectorDistanceMetric::L2, "flat metric mismatch");
        require(index.size() == 0, "flat should not contain materialized entries");

        auto found = results(index.search(vector_key({0.2, 0.0}), VectorSearchRequest {.top_k = 2}));
        require_records(found, {1, 2}, "flat nearest vector order mismatch");
        require(std::abs(found[0].distance - 0.2) < 0.000001, "flat nearest distance mismatch");

        require(storage.update(20, 1, vector_record("first", Value {VectorValue {10.0, 0.0}})).has_value(), "update stored vector failed");
        require_records(
            results(index.search(vector_key({0.2, 0.0}), VectorSearchRequest {.top_k = 2})),
            {2, 3},
            "flat search should observe storage updates"
        );

        require(storage.erase(20, 2).has_value(), "erase stored vector failed");
        require_records(
            results(index.search(vector_key({0.2, 0.0}), VectorSearchRequest {.top_k = 10})),
            {3, 1},
            "flat search should observe storage deletes and skip nulls"
        );
        require(results(index.search(vector_key({0.2, 0.0}), VectorSearchRequest {.top_k = 0})).empty(), "zero top_k should return no results");
    }
    std::filesystem::remove_all(path);
}

void test_flat_index_validates_dimensions_and_storage()
{
    auto filesystem = filesystem::create_platform_filesystem();
    const auto path = temporary_path();
    {
        storage::StorageEngine storage {path, filesystem};
        FlatIndex index(FlatIndexOptions {
            .collection_id = 20,
            .column_ordinal = 1,
            .dimension = 2,
            .metric = VectorDistanceMetric::Cosine,
        }, storage);

        auto invalid = index.search(vector_key({1.0}), VectorSearchRequest {});
        require(!invalid.has_value(), "dimension mismatch should fail");
        require(invalid.error().code == VectorIndexErrorCode::InvalidDimension, "dimension error code mismatch");

        auto missing_storage = index.search(vector_key({1.0, 0.0}), VectorSearchRequest {});
        require(!missing_storage.has_value(), "missing collection should fail");
        require(missing_storage.error().code == VectorIndexErrorCode::StorageFailure, "storage error code mismatch");

        require(index.insert(vector_key({1.0, 0.0}), 1).has_value(), "flat insert hook should be a no-op");
        require(index.erase(1).has_value(), "flat erase hook should be a no-op");
    }
    std::filesystem::remove_all(path);
}

void test_vector_index_engine_lifecycle()
{
    auto filesystem = filesystem::create_platform_filesystem();
    const auto path = temporary_path();
    {
        storage::StorageEngine storage {path / "storage", filesystem};
        require(storage.create_collection(vectors_schema()).has_value(), "create vector collection failed");
        require(storage.insert(20, vector_record("first", Value {VectorValue {1.0, 0.0}})).has_value(), "insert first vector failed");
        require(storage.insert(20, vector_record("second", Value {VectorValue {0.0, 2.0}})).has_value(), "insert second vector failed");

        meta::entry::VectorIndexEntry entry {
            10, 20, 31, "vectors_embedding",
            meta::entry::VectorIndexKind::Hnsw,
            meta::entry::VectorDistanceMetric::InnerProduct,
            2,
            meta::entry::HnswOptions {
                .max_neighbors = 4,
                .ef_construction = 32,
                .ef_search_default = 32,
                .random_seed = 7,
            },
        };
        VectorIndexEngine engine {path / "indexes", filesystem};

        require(engine.create_index(entry, vectors_schema(), storage).has_value(), "create vector index failed");
        auto view = engine.find_index(10);
        require(view.has_value(), "find vector index failed");
        require(view->column_ordinal == 1 && view->dimension == 2, "vector index descriptor view mismatch");
        require(view->metric == VectorDistanceMetric::InnerProduct, "vector index metric view mismatch");
        require(view->entry_count == 2, "vector index entry count mismatch");
        require(engine.list_indexes(20).size() == 1, "list vector indexes mismatch");
        require_records(
            results(engine.search(10, vector_key({0.0, 1.0}), VectorSearchRequest {.top_k = 1})),
            {2},
            "engine inner product search mismatch"
        );

        meta::entry::VectorIndexEntry invalid {
            11, 20, 31, "invalid", meta::entry::VectorIndexKind::Hnsw,
            meta::entry::VectorDistanceMetric::L2, 3
        };
        auto invalid_created = engine.create_index(invalid, vectors_schema(), storage);
        require(!invalid_created.has_value(), "invalid vector metadata should be rejected");
        require(invalid_created.error().code == VectorIndexErrorCode::InvalidMetadata, "invalid metadata error code mismatch");

        require(engine.drop_index(10).has_value(), "drop vector index failed");
        require(!engine.find_index(10).has_value(), "dropped vector index should be absent");
    }
    std::filesystem::remove_all(path);
}

void test_vector_index_engine_restores_and_rebuilds_all()
{
    auto filesystem = filesystem::create_platform_filesystem();
    const auto path = temporary_path();
    const auto index_directory = path / "indexes";
    {
        meta::CatalogEditor catalog;
        auto database_id = catalog.create_database(meta::CreateDatabaseRequest {.name = "demo"});
        require(database_id.has_value(), "create vector catalog database failed");
        auto collection_id = catalog.create_collection(meta::CreateCollectionRequest {
            .database_id = *database_id,
            .name = "vectors",
            .columns = {
                meta::ColumnDefinition {.name = "name", .type = {LogicalTypeId::Varchar, 64}},
                meta::ColumnDefinition {.name = "embedding", .type = {LogicalTypeId::Vector, 2}},
            },
        });
        require(collection_id.has_value(), "create vector catalog collection failed");
        const auto * column = catalog.view().find_column(*collection_id, "embedding");
        require(column != nullptr, "vector catalog column missing");
        auto index_id = catalog.create_vector_index(meta::CreateVectorIndexRequest {
            .collection_id = *collection_id,
            .column_id = column->id(),
            .name = "vectors_embedding",
            .kind = meta::entry::VectorIndexKind::Hnsw,
            .metric = meta::entry::VectorDistanceMetric::L2,
            .hnsw_options = {.max_neighbors = 4, .ef_construction = 32, .ef_search_default = 32, .random_seed = 7},
        });
        require(index_id.has_value(), "create vector catalog index failed");
        auto collection_schema = storage::load_collection_schema(catalog.view(), *collection_id);
        require(collection_schema.has_value(), "load vector catalog schema failed");
        const auto * index_entry = catalog.view().find_vector_index(*index_id);
        require(index_entry != nullptr, "vector catalog index entry missing");

        storage::StorageEngine storage {path / "storage", filesystem};
        require(storage.create_collection(*collection_schema).has_value(), "create hnsw engine collection failed");
        require(storage.insert(*collection_id, vector_record("first", Value {VectorValue {0.0, 0.0}})).has_value(), "insert first engine vector failed");
        require(storage.insert(*collection_id, vector_record("second", Value {VectorValue {1.0, 0.0}})).has_value(), "insert second engine vector failed");
        require(storage.insert(*collection_id, vector_record("third", Value {VectorValue {5.0, 0.0}})).has_value(), "insert third engine vector failed");

        {
            VectorIndexEngine engine {index_directory, filesystem};
            require(engine.create_index(*index_entry, *collection_schema, storage).has_value(), "engine create hnsw index failed");
            require(engine.find_index(*index_id)->entry_count == 3, "engine did not build hnsw from storage");
            require_records(
                results(engine.search(*index_id, vector_key({0.2, 0.0}), VectorSearchRequest {.top_k = 2})),
                {1, 2},
                "engine-created hnsw search mismatch"
            );
        }
        const auto index_path = index_directory / ("vindex_" + std::to_string(*index_id) + ".lhnsw");
        require(std::filesystem::exists(index_path), "engine hnsw file is missing");
        {
            VectorIndexEngine restored {index_directory, filesystem};
            require(restored.restore_all(catalog.view(), storage).has_value(), "engine restore_all failed");
            require_records(
                results(restored.search(*index_id, vector_key({0.2, 0.0}), VectorSearchRequest {.top_k = 2})),
                {1, 2},
                "engine-restored hnsw search mismatch"
            );

            storage::StorageEngine missing_storage {path / "missing-storage", filesystem};
            auto failed = restored.restore_all(catalog.view(), missing_storage);
            require(!failed.has_value(), "restore_all should reject missing collection storage");
            require(restored.find_index(*index_id).has_value(), "failed restore_all should preserve prior engine state");
        }
        require(filesystem.remove(index_path).has_value(), "remove HNSW file for missing-file recovery failed");
        {
            VectorIndexEngine missing {index_directory, filesystem};
            require(missing.restore_all(catalog.view(), storage).has_value(), "engine should rebuild a missing HNSW file");
            require(missing.find_index(*index_id)->entry_count == 3, "missing-file rebuild size mismatch");
        }
        require(storage.insert(*collection_id, vector_record("fourth", Value {VectorValue {0.1, 0.0}})).has_value(), "insert stale storage vector failed");
        {
            VectorIndexEngine stale {index_directory, filesystem};
            require(stale.restore_all(catalog.view(), storage).has_value(), "engine should rebuild stale HNSW index");
            require(stale.find_index(*index_id)->entry_count == 4, "rebuilt HNSW index size mismatch");
            require(stale.drop_index(*index_id).has_value(), "engine drop hnsw index failed");
        }
        require(!std::filesystem::exists(index_path), "engine did not remove hnsw file");
    }
    std::filesystem::remove_all(path);
}

void test_vector_index_key()
{
    auto key = VectorIndexKey::from_value(Value {VectorValue {1.0, 2.0}});
    require(key.has_value(), "vector key extraction failed");
    require(key->dimension() == 2, "vector key dimension mismatch");
    require(key->value() == VectorValue({1.0, 2.0}), "vector key value mismatch");

    auto scalar = VectorIndexKey::from_value(Value {1.0});
    require(!scalar.has_value(), "scalar value should not become vector key");
    require(scalar.error().code == VectorIndexErrorCode::InvalidDimension, "scalar key error code mismatch");

    auto empty = VectorIndexKey::from_vector({});
    require(!empty.has_value(), "empty vector should not become vector key");
    require(empty.error().code == VectorIndexErrorCode::EmptyQuery, "empty vector key error code mismatch");
}

} // namespace

int main()
{
    try {
        test_hnsw_index_persists_search_and_tombstones();
        test_hnsw_store_recovers_truncated_tail();
        test_hnsw_store_rejects_corrupted_commit();
        test_hnsw_rejects_descriptor_mismatch();
        test_hnsw_matches_brute_force_top_one();
        test_flat_index_scans_storage();
        test_flat_index_validates_dimensions_and_storage();
        test_vector_index_engine_lifecycle();
        test_vector_index_engine_restores_and_rebuilds_all();
        test_vector_index_key();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
