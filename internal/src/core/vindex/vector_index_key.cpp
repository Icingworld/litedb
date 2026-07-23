#include "core/vindex/vector_index_key.hpp"

#include <string>
#include <utility>

namespace litedb::core::vindex
{

namespace
{

/**
 * @brief 创建向量索引错误
 * @param code 错误码
 * @param message 错误消息
 * @return 向量索引错误
 */
[[nodiscard]]
VectorIndexError make_error(VectorIndexErrorCode code, std::string message)
{
    return VectorIndexError {code, std::move(message)};
}

} // namespace

VectorIndexKey::VectorIndexKey(common::VectorValue vector)
    : value_(std::move(vector))
{
}

std::expected<VectorIndexKey, VectorIndexError> VectorIndexKey::from_value(const common::Value & value)
{
    const auto * vector = std::get_if<common::VectorValue>(&value.data());
    if (vector == nullptr) {
        return std::unexpected(make_error(VectorIndexErrorCode::InvalidDimension, "Vector index key expects VECTOR value"));
    }
    return from_vector(*vector);
}

std::expected<VectorIndexKey, VectorIndexError> VectorIndexKey::from_vector(common::VectorValue vector)
{
    if (vector.empty()) {
        return std::unexpected(make_error(VectorIndexErrorCode::EmptyQuery, "Vector index key must not be empty"));
    }
    return VectorIndexKey {std::move(vector)};
}

const common::VectorValue & VectorIndexKey::value() const noexcept
{
    return value_;
}

std::size_t VectorIndexKey::dimension() const noexcept
{
    return value_.size();
}

} // namespace litedb::core::vindex
