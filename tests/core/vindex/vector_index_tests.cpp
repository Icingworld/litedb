#include "core/vindex/flat_index/flat_index.hpp"
#include "core/vindex/vector_index_key.hpp"
#include "core/vindex/vector_index_manager.hpp"

#include <chrono>
#include <cmath>
#include <exception>
#include <filesystem>
#include <iostream>
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
        require(!hnsw.has_value(), "placeholder hnsw index should not be connected");
        require(hnsw.error().code == VectorIndexErrorCode::UnsupportedIndexKind, "hnsw placeholder error code mismatch");
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
        test_flat_index_scans_storage();
        test_flat_index_validates_dimensions_and_storage();
        test_vector_index_manager_lifecycle();
        test_vector_index_key();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
