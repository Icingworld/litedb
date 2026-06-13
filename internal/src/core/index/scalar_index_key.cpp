#include "core/index/scalar_index_key.hpp"

#include <cmath>
#include <functional>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>

namespace litedb::core::index
{

namespace
{

/**
 * @brief 键类型
 */
enum class KeyClass
{
    Null,                 ///< 空
    Boolean,              ///< 布尔
    Numeric,              ///< 数值
    String,               ///< 字符串
};

[[nodiscard]]
IndexError make_index_error(IndexErrorCode code, std::string message)
{
    return IndexError {code, std::move(message)};
}

/**
 * @brief 是否是支持的标量索引键
 * @param value 值
 * @return 是否支持
 */
[[nodiscard]]
bool is_supported_scalar(const schema::Value & value) noexcept
{
    return !std::holds_alternative<schema::VectorValue>(value.data());
}

/**
 * @brief 是否是数值
 * @param value 值
 * @return 是否是数值
 */
[[nodiscard]]
bool is_numeric_value(const schema::Value & value) noexcept
{
    return std::holds_alternative<std::int32_t>(value.data())
        || std::holds_alternative<std::int64_t>(value.data())
        || std::holds_alternative<float>(value.data())
        || std::holds_alternative<double>(value.data());
}

/**
 * @brief 获取键类型
 * @param value 值
 * @return 键类型
 */
[[nodiscard]]
KeyClass key_class(const schema::Value & value) noexcept
{
    if (value.is_null()) {
        return KeyClass::Null;
    }
    if (std::holds_alternative<bool>(value.data())) {
        return KeyClass::Boolean;
    }
    if (is_numeric_value(value)) {
        return KeyClass::Numeric;
    }
    return KeyClass::String;
}

/**
 * @brief 获取数值
 * @param value 值
 * @return 数值
 */
[[nodiscard]]
long double numeric_value(const schema::Value & value) noexcept
{
    return std::visit(
        [](const auto & data) -> long double {
            using T = std::decay_t<decltype(data)>;
            if constexpr (std::is_same_v<T, std::int32_t>
                || std::is_same_v<T, std::int64_t>
                || std::is_same_v<T, float>
                || std::is_same_v<T, double>) {
                return static_cast<long double>(data);
            } else {
                return 0.0L;
            }
        },
        value.data()
    );
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
    // 不支持使用向量值构造索引
    if (!is_supported_scalar(value)) [[unlikely]] {
        return std::unexpected(make_index_error(
            IndexErrorCode::UnsupportedKeyType,
            "Vector values cannot be used as scalar index keys"
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
    const auto left_class = key_class(left.value());
    const auto right_class = key_class(right.value());

    // 如果键类型不同，则直接比较键类型
    if (left_class != right_class) {
        return static_cast<std::uint8_t>(left_class) < static_cast<std::uint8_t>(right_class)
            ? std::strong_ordering::less
            : std::strong_ordering::greater;
    }

    // 键类型相同，则比较值
    switch (left_class) {
    case KeyClass::Null:
        return std::strong_ordering::equal;
    case KeyClass::Boolean: {
        const auto left_bool = std::get<bool>(left.value().data());
        const auto right_bool = std::get<bool>(right.value().data());

        return left_bool == right_bool
            ? std::strong_ordering::equal
            : (left_bool ? std::strong_ordering::greater : std::strong_ordering::less);
    }
    case KeyClass::Numeric: {
        const auto left_number = numeric_value(left.value());
        const auto right_number = numeric_value(right.value());
        const auto left_nan = std::isnan(left_number);
        const auto right_nan = std::isnan(right_number);

        if (left_nan || right_nan) {
            if (left_nan && right_nan) {
                // NaN == NaN
                return std::strong_ordering::equal;
            }

            // Nan > 非 Nan
            return left_nan ? std::strong_ordering::greater : std::strong_ordering::less;
        }

        // 比较数值
        if (left_number < right_number) {
            return std::strong_ordering::less;
        }
        if (right_number < left_number) {
            return std::strong_ordering::greater;
        }
        return std::strong_ordering::equal;
    }
    case KeyClass::String: {
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
    const auto key_kind = key_class(key.value());
    auto seed = std::hash<std::uint8_t> {}(static_cast<std::uint8_t>(key_kind));

    switch (key_kind) {
    case KeyClass::Null:
        return seed;
    case KeyClass::Boolean:
        return hash_combine(seed, std::hash<bool> {}(std::get<bool>(key.value().data())));
    case KeyClass::Numeric: {
        const auto number = numeric_value(key.value());
        if (std::isnan(number)) {
            return hash_combine(seed, std::numeric_limits<std::size_t>::max());
        }
        return hash_combine(seed, std::hash<long double> {}(number));
    }
    case KeyClass::String:
        return hash_combine(seed, std::hash<std::string> {}(std::get<std::string>(key.value().data())));
    }

    return seed;
}

bool ScalarIndexLess::operator()(const ScalarIndexKey & left, const ScalarIndexKey & right) const noexcept
{
    return compare_scalar_index_keys(left, right) == std::strong_ordering::less;
}

} // namespace litedb::core::index
