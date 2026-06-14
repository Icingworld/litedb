#include "core/vindex/hnsw_index.hpp"
#include "core/vindex/vector_index_key.hpp"
#include "core/vindex/vector_index_manager.hpp"

#include <cmath>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{

using namespace litedb::core::common;
using namespace litedb::core::schema;
using namespace litedb::core::vindex;

void require(bool condition, const char * message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
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

void test_hnsw_index_searches_nearest_vectors()
{
    HnswIndex index(HnswIndexOptions {
        .dimension = 2,
        .metric = VectorDistanceMetric::L2,
    });

    require(index.kind() == VectorIndexKind::Hnsw, "hnsw kind mismatch");
    require(index.dimension() == 2, "hnsw dimension mismatch");
    require(index.insert(VectorValue {0.0, 0.0}, 1).has_value(), "insert first vector failed");
    require(index.insert(VectorValue {1.0, 0.0}, 2).has_value(), "insert second vector failed");
    require(index.insert(VectorValue {5.0, 0.0}, 3).has_value(), "insert third vector failed");
    require(index.size() == 3, "hnsw size mismatch");

    auto found = results(index.search(VectorValue {0.2, 0.0}, VectorSearchParameters {.limit = 2}));
    require_records(found, {1, 2}, "nearest vector order mismatch");
    require(std::abs(found[0].distance - 0.2) < 0.000001, "nearest distance mismatch");

    require(index.update(VectorValue {10.0, 0.0}, 1).has_value(), "update vector failed");
    require_records(
        results(index.search(VectorValue {0.2, 0.0}, VectorSearchParameters {.limit = 2})),
        {2, 3},
        "updated nearest vector order mismatch"
    );

    require(index.erase(2).has_value(), "erase vector failed");
    require(index.size() == 2, "hnsw size after erase mismatch");
}

void test_hnsw_index_validates_dimensions_and_duplicates()
{
    HnswIndex index(HnswIndexOptions {
        .dimension = 3,
        .metric = VectorDistanceMetric::Cosine,
    });

    auto invalid_insert = index.insert(VectorValue {1.0, 2.0}, 1);
    require(!invalid_insert.has_value(), "dimension mismatch should fail");
    require(invalid_insert.error().code == VectorIndexErrorCode::InvalidDimension, "dimension error code mismatch");

    require(index.insert(VectorValue {1.0, 0.0, 0.0}, 1).has_value(), "insert vector failed");
    auto duplicate = index.insert(VectorValue {0.0, 1.0, 0.0}, 1);
    require(!duplicate.has_value(), "duplicate record should fail");
    require(duplicate.error().code == VectorIndexErrorCode::RecordAlreadyExists, "duplicate error code mismatch");

    auto missing = index.erase(2);
    require(!missing.has_value(), "missing record erase should fail");
    require(missing.error().code == VectorIndexErrorCode::RecordNotFound, "missing record error code mismatch");
}

void test_vector_index_manager_lifecycle()
{
    VectorIndexManager manager;
    VectorIndexDefinition definition {
        .index_id = 10,
        .collection_id = 20,
        .column_id = 30,
        .kind = VectorIndexKind::Hnsw,
        .hnsw_options = HnswIndexOptions {
            .dimension = 2,
            .metric = VectorDistanceMetric::InnerProduct,
        },
    };

    require(manager.create_index(definition).has_value(), "create vector index failed");
    require(manager.find_index(10).has_value(), "find vector index failed");
    require(manager.list_indexes(20).size() == 1, "list vector indexes mismatch");

    require(manager.insert(10, VectorValue {1.0, 0.0}, 100).has_value(), "manager insert first vector failed");
    require(manager.insert(10, VectorValue {0.0, 2.0}, 200).has_value(), "manager insert second vector failed");
    require_records(
        results(manager.search(10, VectorValue {0.0, 1.0}, VectorSearchParameters {.limit = 1})),
        {200},
        "manager inner product search mismatch"
    );

    require(manager.drop_index(10).has_value(), "drop vector index failed");
    require(!manager.find_index(10).has_value(), "dropped vector index should be absent");
}

void test_vector_key_from_value()
{
    auto key = vector_key_from_value(Value {VectorValue {1.0, 2.0}});
    require(key.has_value(), "vector key extraction failed");
    require(key.value().size() == 2, "vector key dimension mismatch");

    auto scalar = vector_key_from_value(Value {1.0});
    require(!scalar.has_value(), "scalar value should not become vector key");
    require(scalar.error().code == VectorIndexErrorCode::InvalidDimension, "scalar key error code mismatch");
}

} // namespace

int main()
{
    try {
        test_hnsw_index_searches_nearest_vectors();
        test_hnsw_index_validates_dimensions_and_duplicates();
        test_vector_index_manager_lifecycle();
        test_vector_key_from_value();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
