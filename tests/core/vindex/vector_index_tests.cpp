#include "core/vindex/flat_index/flat_index.hpp"
#include "core/vindex/hnsw_index/hnsw_index.hpp"
#include "core/vindex/vector_index_key.hpp"
#include "core/vindex/vector_index_manager.hpp"

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
#include "core/schema/collection.hpp"
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
            .access = filesystem::backend::FileAccess::ReadWrite,
            .create_mode = filesystem::backend::FileCreateMode::OpenExisting,
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
            .access = filesystem::backend::FileAccess::ReadWrite,
            .create_mode = filesystem::backend::FileCreateMode::OpenExisting,
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
    require(opened.error().code == VectorIndexErrorCode::StorageFailure, "corrupted hnsw error code mismatch");
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
    require(opened.error().code == VectorIndexErrorCode::StorageFailure, "descriptor mismatch error code mismatch");
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

    std::vector<schema::VectorValue> vectors;
    vectors.reserve(128);
    for (std::size_t index = 0; index < 128; ++index) {
        const auto x = std::sin(static_cast<double>(index) * 0.37) * 10.0;
        const auto y = std::cos(static_cast<double>(index) * 0.61) * 10.0;
        vectors.push_back({x, y});
        require(created->insert(vector_key({x, y}), index + 1).has_value(), "insert recall vector failed");
    }

    std::size_t matches = 0;
    for (std::size_t query_index = 0; query_index < 32; ++query_index) {
        const schema::VectorValue query {
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

void test_vector_index_manager_lifecycle()
{
    auto filesystem = filesystem::create_platform_filesystem();
    const auto path = temporary_path();
    {
        storage::StorageEngine storage {path, filesystem};
        require(storage.create_collection(vectors_schema()).has_value(), "create vector collection failed");
        require(storage.insert(20, vector_record("first", Value {VectorValue {1.0, 0.0}})).has_value(), "insert first vector failed");
        require(storage.insert(20, vector_record("second", Value {VectorValue {0.0, 2.0}})).has_value(), "insert second vector failed");

        VectorIndexManager manager {storage};
        VectorIndexDefinition definition {
            .index_id = 10,
            .collection_id = 20,
            .column_id = 31,
            .column_ordinal = 1,
            .dimension = 2,
            .kind = VectorIndexKind::Flat,
            .metric = VectorDistanceMetric::InnerProduct,
        };

        require(manager.create_index(definition).has_value(), "create flat index failed");
        require(manager.find_index(10).has_value(), "find flat index failed");
        require(manager.list_indexes(20).size() == 1, "list flat indexes mismatch");
        require_records(
            results(manager.search(10, vector_key({0.0, 1.0}), VectorSearchRequest {.top_k = 1})),
            {2},
            "manager inner product search mismatch"
        );

        require(manager.drop_index(10).has_value(), "drop flat index failed");
        require(!manager.find_index(10).has_value(), "dropped flat index should be absent");

        auto hnsw = manager.create_index(VectorIndexDefinition {
            .index_id = 11,
            .collection_id = 20,
            .column_id = 31,
            .column_ordinal = 1,
            .dimension = 2,
            .kind = VectorIndexKind::Hnsw,
        });
        require(!hnsw.has_value(), "non-persistent manager should reject HNSW creation");
        require(hnsw.error().code == VectorIndexErrorCode::StorageFailure, "non-persistent manager hnsw error code mismatch");
    }
    std::filesystem::remove_all(path);
}

void test_vector_index_manager_creates_and_restores_hnsw()
{
    auto filesystem = filesystem::create_platform_filesystem();
    const auto path = temporary_path();
    const auto index_directory = path / "indexes";
    {
        storage::StorageEngine storage {path / "storage", filesystem};
        require(storage.create_collection(vectors_schema()).has_value(), "create hnsw manager collection failed");
        require(storage.insert(20, vector_record("first", Value {VectorValue {0.0, 0.0}})).has_value(), "insert first manager vector failed");
        require(storage.insert(20, vector_record("second", Value {VectorValue {1.0, 0.0}})).has_value(), "insert second manager vector failed");
        require(storage.insert(20, vector_record("third", Value {VectorValue {5.0, 0.0}})).has_value(), "insert third manager vector failed");

        VectorIndexDefinition definition {
            .index_id = 40,
            .collection_id = 20,
            .column_id = 31,
            .column_ordinal = 1,
            .dimension = 2,
            .kind = VectorIndexKind::Hnsw,
            .metric = VectorDistanceMetric::L2,
            .max_neighbors = 4,
            .ef_construction = 32,
            .ef_search_default = 32,
            .random_seed = 7,
        };
        {
            VectorIndexManager manager {index_directory, filesystem, storage};
            require(manager.create_index(definition).has_value(), "manager create hnsw index failed");
            require(manager.find_index(40)->index.size() == 3, "manager did not build hnsw from storage");
            require_records(
                results(manager.search(40, vector_key({0.2, 0.0}), VectorSearchRequest {.top_k = 2})),
                {1, 2},
                "manager-created hnsw search mismatch"
            );
        }
        require(std::filesystem::exists(index_directory / "vindex_40.lhnsw"), "manager hnsw file is missing");
        {
            VectorIndexManager restored {index_directory, filesystem, storage};
            require(restored.restore_index(definition).has_value(), "manager restore hnsw index failed");
            require_records(
                results(restored.search(40, vector_key({0.2, 0.0}), VectorSearchRequest {.top_k = 2})),
                {1, 2},
                "manager-restored hnsw search mismatch"
            );
        }
        require(storage.insert(20, vector_record("fourth", Value {VectorValue {0.1, 0.0}})).has_value(), "insert stale storage vector failed");
        {
            VectorIndexManager stale {index_directory, filesystem, storage};
            auto restored = stale.restore_index(definition);
            require(!restored.has_value(), "manager should detect an HNSW index stale against storage");
            require(stale.rebuild_index(definition).has_value(), "manager rebuild of stale HNSW index failed");
            require(stale.find_index(40)->index.size() == 4, "rebuilt HNSW index size mismatch");
            require(stale.drop_index(40).has_value(), "manager drop hnsw index failed");
        }
        require(!std::filesystem::exists(index_directory / "vindex_40.lhnsw"), "manager did not remove hnsw file");
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
        test_vector_index_manager_lifecycle();
        test_vector_index_manager_creates_and_restores_hnsw();
        test_vector_index_key();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
