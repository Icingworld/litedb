#include "core/vindex/flat_index/flat_index.hpp"

#include <algorithm>
#include <queue>
#include <string>
#include <utility>

#include "core/storage/storage_engine.hpp"
#include "core/vindex/vector_distance.hpp"

namespace litedb::core::vindex
{

namespace
{

[[nodiscard]]
VectorIndexError make_error(VectorIndexErrorCode code, std::string message)
{
    return VectorIndexError {code, message};
}

[[nodiscard]]
VectorIndexError storage_error(error::Error source)
{
    return VectorIndexError {
        VectorIndexErrorCode::StorageFailure,
        source.message(),
        VectorIndexErrorContext {
            .operation = VectorIndexOperation::Search,
            .source_code = source.encode_code(),
        },
    };
}

[[nodiscard]]
bool result_less(const VectorSearchResult & left, const VectorSearchResult & right) noexcept
{
    if (left.distance != right.distance) {
        return left.distance < right.distance;
    }
    return left.record_id < right.record_id;
}

struct ResultLess
{
    [[nodiscard]]
    bool operator()(const VectorSearchResult & left, const VectorSearchResult & right) const noexcept
    {
        return result_less(left, right);
    }
};

} // namespace

FlatIndex::FlatIndex(FlatIndexOptions options, const storage::StorageEngine & storage) noexcept
    : options_(options)
    , storage_(&storage)
{
}

VectorIndexKind FlatIndex::kind() const noexcept
{
    return VectorIndexKind::Flat;
}

VectorDistanceMetric FlatIndex::metric() const noexcept
{
    return options_.metric;
}

std::size_t FlatIndex::dimension() const noexcept
{
    return options_.dimension;
}

std::expected<void, VectorIndexError> FlatIndex::insert(
    const VectorIndexKey & key,
    common::RecordId
)
{
    return validate_key(key);
}

std::expected<void, VectorIndexError> FlatIndex::erase(common::RecordId)
{
    return {};
}

std::expected<std::vector<VectorSearchResult>, VectorIndexError> FlatIndex::search(
    const VectorIndexKey & query,
    VectorSearchRequest request
) const
{
    auto validation = validate_key(query);
    if (!validation.has_value()) {
        return std::unexpected(std::move(validation.error()));
    }
    if (request.top_k == 0) {
        return std::vector<VectorSearchResult> {};
    }

    auto cursor = storage_->scan(options_.collection_id);
    if (!cursor.has_value()) {
        return std::unexpected(storage_error(std::move(cursor.error())));
    }

    std::priority_queue<VectorSearchResult, std::vector<VectorSearchResult>, ResultLess> nearest;
    while (true) {
        auto next = cursor->next();
        if (!next.has_value()) {
            return std::unexpected(storage_error(std::move(next.error())));
        }
        if (!next->has_value()) {
            break;
        }

        const auto & record = next->value();
        if (options_.column_ordinal >= record.data.values.size()) {
            return std::unexpected(make_error(
                VectorIndexErrorCode::StorageFailure,
                "Vector column ordinal is outside the stored record"
            ));
        }

        const auto & value = record.data.values[options_.column_ordinal];
        if (value.is_null()) {
            continue;
        }

        auto key = VectorIndexKey::from_value(value);
        if (!key.has_value()) {
            return std::unexpected(std::move(key.error()));
        }
        auto key_validation = validate_key(*key);
        if (!key_validation.has_value()) {
            return std::unexpected(std::move(key_validation.error()));
        }

        auto distance = vector_distance(query.value(), key->value(), options_.metric);
        if (!distance.has_value()) {
            return std::unexpected(std::move(distance.error()));
        }

        VectorSearchResult candidate {
            .record_id = record.record_id,
            .distance = *distance,
        };
        if (nearest.size() < request.top_k) {
            nearest.push(candidate);
        } else if (result_less(candidate, nearest.top())) {
            nearest.pop();
            nearest.push(candidate);
        }
    }

    std::vector<VectorSearchResult> results;
    results.reserve(nearest.size());
    while (!nearest.empty()) {
        results.push_back(nearest.top());
        nearest.pop();
    }
    std::sort(results.begin(), results.end(), result_less);
    return results;
}

std::size_t FlatIndex::size() const noexcept
{
    return 0;
}

const FlatIndexOptions & FlatIndex::options() const noexcept
{
    return options_;
}

std::expected<void, VectorIndexError> FlatIndex::validate_key(const VectorIndexKey & key) const
{
    if (options_.dimension == 0 || key.dimension() != options_.dimension) {
        return std::unexpected(make_error(
            VectorIndexErrorCode::InvalidDimension,
            "Vector dimension does not match index dimension"
        ));
    }
    return {};
}

} // namespace litedb::core::vindex
