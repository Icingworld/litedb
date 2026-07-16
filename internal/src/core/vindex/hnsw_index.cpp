#include "core/vindex/hnsw_index.hpp"

#include <algorithm>
#include <string>
#include <utility>

#include "core/vindex/vector_distance.hpp"

namespace litedb::core::vindex
{

namespace
{

[[nodiscard]]
VectorIndexError make_error(VectorIndexErrorCode code, std::string message)
{
    return VectorIndexError {code, std::move(message)};
}

} // namespace

HnswIndex::HnswIndex(HnswIndexOptions options)
    : options_(options)
    , vectors_()
{
}

VectorIndexKind HnswIndex::kind() const noexcept
{
    return VectorIndexKind::Hnsw;
}

VectorDistanceMetric HnswIndex::metric() const noexcept
{
    return options_.metric;
}

std::size_t HnswIndex::dimension() const noexcept
{
    return options_.dimension;
}

std::expected<void, VectorIndexError> HnswIndex::insert(
    const VectorIndexKey & key,
    common::RecordId record_id
)
{
    auto validation = validate_key(key);
    if (!validation.has_value()) {
        return validation;
    }
    if (vectors_.contains(record_id)) {
        return std::unexpected(make_error(VectorIndexErrorCode::RecordAlreadyExists, "Vector record already exists"));
    }

    vectors_.emplace(record_id, key.value());
    return {};
}

std::expected<void, VectorIndexError> HnswIndex::erase(common::RecordId record_id)
{
    if (vectors_.erase(record_id) == 0) {
        return std::unexpected(make_error(VectorIndexErrorCode::RecordNotFound, "Vector record not found"));
    }
    return {};
}

std::expected<void, VectorIndexError> HnswIndex::update(
    const VectorIndexKey & key,
    common::RecordId record_id
)
{
    auto validation = validate_key(key);
    if (!validation.has_value()) {
        return validation;
    }

    auto found = vectors_.find(record_id);
    if (found == vectors_.end()) {
        return std::unexpected(make_error(VectorIndexErrorCode::RecordNotFound, "Vector record not found"));
    }

    found->second = key.value();
    return {};
}

std::expected<std::vector<VectorSearchResult>, VectorIndexError> HnswIndex::search(
    const VectorIndexKey & query,
    VectorSearchParameters parameters
) const
{
    auto validation = validate_key(query);
    if (!validation.has_value()) {
        return std::unexpected(std::move(validation.error()));
    }
    if (parameters.limit == 0 || vectors_.empty()) {
        return std::vector<VectorSearchResult> {};
    }

    std::vector<VectorSearchResult> results;
    results.reserve(vectors_.size());
    for (const auto & [record_id, vector] : vectors_) {
        auto distance = vector_distance(query.value(), vector, options_.metric);
        if (!distance.has_value()) {
            return std::unexpected(std::move(distance.error()));
        }
        results.push_back(VectorSearchResult {
            .record_id = record_id,
            .distance = distance.value(),
        });
    }

    std::sort(results.begin(), results.end(), [](const auto & left, const auto & right) {
        if (left.distance == right.distance) {
            return left.record_id < right.record_id;
        }
        return left.distance < right.distance;
    });

    if (results.size() > parameters.limit) {
        results.resize(parameters.limit);
    }
    return results;
}

void HnswIndex::clear() noexcept
{
    vectors_.clear();
}

std::size_t HnswIndex::size() const noexcept
{
    return vectors_.size();
}

const HnswIndexOptions & HnswIndex::options() const noexcept
{
    return options_;
}

std::expected<void, VectorIndexError> HnswIndex::validate_key(const VectorIndexKey & key) const
{
    if (options_.dimension == 0 || key.dimension() != options_.dimension) {
        return std::unexpected(make_error(VectorIndexErrorCode::InvalidDimension, "Vector dimension does not match index dimension"));
    }
    return {};
}

} // namespace litedb::core::vindex
