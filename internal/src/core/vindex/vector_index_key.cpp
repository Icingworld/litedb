#include "core/vindex/vector_index_key.hpp"

#include <string>
#include <utility>

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

std::expected<schema::VectorValue, VectorIndexError> vector_key_from_value(const schema::Value & value)
{
    const auto * vector = std::get_if<schema::VectorValue>(&value.data());
    if (vector == nullptr) {
        return std::unexpected(make_error(VectorIndexErrorCode::InvalidDimension, "Vector index key expects VECTOR value"));
    }
    if (vector->empty()) {
        return std::unexpected(make_error(VectorIndexErrorCode::EmptyQuery, "Vector index key must not be empty"));
    }
    return *vector;
}

} // namespace litedb::core::vindex
