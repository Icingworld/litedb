#include "core/index/scalar_index_key.hpp"

#include <cmath>
#include <functional>
#include <string>
#include <utility>

namespace litedb::core::index
{

namespace
{

/**
 * @brief 键类型
 */
enum class KeyType
{
    Boolean,        ///< 布尔值
    Integer,        ///< 整数
    BigInt,         ///< 大整数
    Float,          ///< 浮点数
    Double,         ///< 双精度浮点数
    Varchar,        ///< 字符串
};

/**
 * @brief 创建索引错误
 * @param code 错误码
 * @param message 错误消息
 * @return 索引错误
 */
[[nodiscard]]
IndexError make_index_error(IndexErrorCode code, std::string message)
{
    return IndexError {code, std::move(message)};
}

/**
 * @brief 是否是向量值
 * @param value 值
 * @return 是否是向量值
 */
[[nodiscard]]
bool is_vector(const schema::Value & value) noexcept
{
    return std::holds_alternative<schema::VectorValue>(value.data());
}

/**
 * @brief 是否是无效浮点键值
 * @param value 值
 * @return 是否是 NaN
 */
[[nodiscard]]
bool is_nan(const schema::Value & value) noexcept
{
    if (const auto * number = std::get_if<float>(&value.data())) {
        return std::isnan(*number);
    }
    if (const auto * number = std::get_if<double>(&value.data())) {
        return std::isnan(*number);
    }
    return false;
}

/**
 * @brief 获取键类型
 * @param value 值
 * @return 键类型
 */
[[nodiscard]]
KeyType key_type(const schema::Value & value) noexcept
{
    if (std::holds_alternative<bool>(value.data())) {
        return KeyType::Boolean;
    }
    if (std::holds_alternative<std::int32_t>(value.data())) {
        return KeyType::Integer;
    }
    if (std::holds_alternative<std::int64_t>(value.data())) {
        return KeyType::BigInt;
    }
    if (std::holds_alternative<float>(value.data())) {
        return KeyType::Float;
    }
    if (std::holds_alternative<double>(value.data())) {
        return KeyType::Double;
    }
    return KeyType::Varchar;
}

/**
 * @brief 合并哈希值
 * @param seed 种子
 * @param value 值
 * @return 合并后的哈希值
 */
[[nodiscard]]
std::size_t hash_combine(std::size_t seed, std::size_t value) noexcept
{
    return seed ^ (value + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U));
}

} // namespace

ScalarIndexKey::ScalarIndexKey(schema::Value value)
    : value_(std::move(value))
{
}

std::expected<ScalarIndexKey, IndexError> ScalarIndexKey::from_value(schema::Value value)
{
    if (is_vector(value)) [[unlikely]] {
        return std::unexpected(make_index_error(
            IndexErrorCode::UnsupportedKeyType,
            "Vector values cannot be used as scalar index keys"
        ));
    }
    if (value.is_null()) [[unlikely]] {
        return std::unexpected(make_index_error(
            IndexErrorCode::InvalidKeyValue,
            "Null values are not stored in scalar indexes"
        ));
    }
    if (is_nan(value)) [[unlikely]] {
        return std::unexpected(make_index_error(
            IndexErrorCode::InvalidKeyValue,
            "NaN values cannot be used as scalar index keys"
        ));
    }
    return ScalarIndexKey {std::move(value)};
}

const schema::Value & ScalarIndexKey::value() const noexcept
{
    return value_;
}

std::strong_ordering compare_scalar_index_keys(
    const ScalarIndexKey & left,
    const ScalarIndexKey & right
) noexcept
{
    const auto left_type = key_type(left.value());
    const auto right_type = key_type(right.value());
    if (left_type != right_type) {
        return static_cast<std::uint8_t>(left_type) < static_cast<std::uint8_t>(right_type)
            ? std::strong_ordering::less
            : std::strong_ordering::greater;
    }

    switch (left_type) {
    case KeyType::Boolean: {
        const auto left_bool = std::get<bool>(left.value().data());
        const auto right_bool = std::get<bool>(right.value().data());

        return left_bool == right_bool
            ? std::strong_ordering::equal
            : (left_bool ? std::strong_ordering::greater : std::strong_ordering::less);
    }
    case KeyType::Integer: {
        const auto left_number = std::get<std::int32_t>(left.value().data());
        const auto right_number = std::get<std::int32_t>(right.value().data());
        return left_number <=> right_number;
    }
    case KeyType::BigInt: {
        const auto left_number = std::get<std::int64_t>(left.value().data());
        const auto right_number = std::get<std::int64_t>(right.value().data());
        return left_number <=> right_number;
    }
    case KeyType::Float: {
        const auto left_number = std::get<float>(left.value().data());
        const auto right_number = std::get<float>(right.value().data());
        if (left_number < right_number) {
            return std::strong_ordering::less;
        }
        if (right_number < left_number) {
            return std::strong_ordering::greater;
        }
        return std::strong_ordering::equal;
    }
    case KeyType::Double: {
        const auto left_number = std::get<double>(left.value().data());
        const auto right_number = std::get<double>(right.value().data());
        if (left_number < right_number) {
            return std::strong_ordering::less;
        }
        if (right_number < left_number) {
            return std::strong_ordering::greater;
        }
        return std::strong_ordering::equal;
    }
    case KeyType::Varchar: {
        const auto & left_string = std::get<std::string>(left.value().data());
        const auto & right_string = std::get<std::string>(right.value().data());

        // 比较字符串
        if (left_string < right_string) {
            return std::strong_ordering::less;
        }
        if (right_string < left_string) {
            return std::strong_ordering::greater;
        }
        return std::strong_ordering::equal;
    }
    }

    return std::strong_ordering::equal;
}

bool ScalarIndexEqual::operator()(const ScalarIndexKey & left, const ScalarIndexKey & right) const noexcept
{
    return compare_scalar_index_keys(left, right) == std::strong_ordering::equal;
}

std::size_t ScalarIndexHash::operator()(const ScalarIndexKey & key) const noexcept
{
    const auto type = key_type(key.value());
    auto seed = std::hash<std::uint8_t> {}(static_cast<std::uint8_t>(type));

    switch (type) {
    case KeyType::Boolean:
        return hash_combine(seed, std::hash<bool> {}(std::get<bool>(key.value().data())));
    case KeyType::Integer:
        return hash_combine(seed, std::hash<std::int32_t> {}(std::get<std::int32_t>(key.value().data())));
    case KeyType::BigInt:
        return hash_combine(seed, std::hash<std::int64_t> {}(std::get<std::int64_t>(key.value().data())));
    case KeyType::Float:
        return hash_combine(seed, std::hash<float> {}(std::get<float>(key.value().data())));
    case KeyType::Double:
        return hash_combine(seed, std::hash<double> {}(std::get<double>(key.value().data())));
    case KeyType::Varchar:
        return hash_combine(seed, std::hash<std::string> {}(std::get<std::string>(key.value().data())));
    }

    return seed;
}

bool ScalarIndexLess::operator()(const ScalarIndexKey & left, const ScalarIndexKey & right) const noexcept
{
    return compare_scalar_index_keys(left, right) == std::strong_ordering::less;
}

} // namespace litedb::core::index
