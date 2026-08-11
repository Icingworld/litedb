#include "core/evaluator/value_operations.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "core/evaluator/evaluator_helper.hpp"

namespace litedb::core::evaluator
{

namespace
{

// 将运行时值解释为 SQL 三值布尔值
[[nodiscard]]
std::expected<std::optional<bool>, EvaluationError> boolean_value(const common::Value & value)
{
    if (value.is_null()) {
        return std::optional<bool> {};
    }
    if (const auto * boolean = std::get_if<bool>(&value.data())) {
        return std::optional<bool> {*boolean};
    }
    return std::unexpected(make_error(EvaluationErrorCode::InvalidType, "Expected BOOLEAN value"));
}

// 执行带溢出检查的有符号整数加法
template <typename T>
[[nodiscard]]
std::expected<T, EvaluationError> checked_add(T left, T right)
{
    constexpr auto minimum = std::numeric_limits<T>::min();
    constexpr auto maximum = std::numeric_limits<T>::max();
    if ((right > 0 && left > maximum - right) || (right < 0 && left < minimum - right)) {
        return std::unexpected(
            make_error(EvaluationErrorCode::NumericOverflow, "Integer addition overflow")
        );
    }
    return static_cast<T>(left + right);
}

// 执行带溢出检查的有符号整数减法
template <typename T>
[[nodiscard]]
std::expected<T, EvaluationError> checked_subtract(T left, T right)
{
    constexpr auto minimum = std::numeric_limits<T>::min();
    constexpr auto maximum = std::numeric_limits<T>::max();
    if ((right > 0 && left < minimum + right) || (right < 0 && left > maximum + right)) {
        return std::unexpected(
            make_error(EvaluationErrorCode::NumericOverflow, "Integer subtraction overflow")
        );
    }
    return static_cast<T>(left - right);
}

// 执行带溢出检查的有符号整数乘法
template <typename T>
[[nodiscard]]
std::expected<T, EvaluationError> checked_multiply(T left, T right)
{
    constexpr auto minimum = std::numeric_limits<T>::min();
    constexpr auto maximum = std::numeric_limits<T>::max();
    if (left == 0 || right == 0) {
        return T {0};
    }
    if ((left > 0 && right > 0 && left > maximum / right) ||
        (left > 0 && right < 0 && right < minimum / left) ||
        (left < 0 && right > 0 && left < minimum / right) ||
        (left < 0 && right < 0 && left < maximum / right)) {
        return std::unexpected(
            make_error(EvaluationErrorCode::NumericOverflow, "Integer multiplication overflow")
        );
    }
    return static_cast<T>(left * right);
}

// 执行带除零与溢出检查的有符号整数除法
template <typename T>
[[nodiscard]]
std::expected<T, EvaluationError> checked_divide(T left, T right)
{
    if (right == 0) {
        return std::unexpected(make_error(EvaluationErrorCode::DivisionByZero, "Division by zero"));
    }
    if (left == std::numeric_limits<T>::min() && right == T {-1}) {
        return std::unexpected(
            make_error(EvaluationErrorCode::NumericOverflow, "Integer division overflow")
        );
    }
    return static_cast<T>(left / right);
}

// 执行带除零检查的有符号整数取模
// 最小值对 -1 取模在数学上恒为零，单独处理以避免原生运算的未定义行为
template <typename T>
[[nodiscard]]
std::expected<T, EvaluationError> checked_modulus(T left, T right)
{
    if (right == 0) {
        return std::unexpected(make_error(EvaluationErrorCode::DivisionByZero, "Division by zero"));
    }
    if (left == std::numeric_limits<T>::min() && right == T {-1}) {
        return T {0};
    }
    return static_cast<T>(left % right);
}

// 求值整数二元算术表达式
template <typename T>
[[nodiscard]]
std::expected<common::Value, EvaluationError>
evaluate_integer_binary(common::BinaryOperator op, T left, T right)
{
    std::expected<T, EvaluationError> result = std::unexpected(
        make_error(EvaluationErrorCode::UnsupportedExpression, "Unsupported integer operator")
    );
    switch (op) {
    case common::BinaryOperator::Add:
        result = checked_add(left, right);
        break;
    case common::BinaryOperator::Subtract:
        result = checked_subtract(left, right);
        break;
    case common::BinaryOperator::Multiply:
        result = checked_multiply(left, right);
        break;
    case common::BinaryOperator::Divide:
        result = checked_divide(left, right);
        break;
    case common::BinaryOperator::Modulus:
        result = checked_modulus(left, right);
        break;
    default:
        break;
    }
    if (!result.has_value()) {
        return std::unexpected(std::move(result.error()));
    }
    return common::Value {common::ValueData {*result}};
}

// 求值浮点二元算术表达式
template <typename T>
[[nodiscard]]
std::expected<common::Value, EvaluationError>
evaluate_floating_binary(common::BinaryOperator op, T left, T right)
{
    if ((op == common::BinaryOperator::Divide || op == common::BinaryOperator::Modulus) &&
        right == T {0}) {
        return std::unexpected(make_error(EvaluationErrorCode::DivisionByZero, "Division by zero"));
    }

    T result {};
    switch (op) {
    case common::BinaryOperator::Add:
        result = left + right;
        break;
    case common::BinaryOperator::Subtract:
        result = left - right;
        break;
    case common::BinaryOperator::Multiply:
        result = left * right;
        break;
    case common::BinaryOperator::Divide:
        result = left / right;
        break;
    case common::BinaryOperator::Modulus:
        result = std::fmod(left, right);
        break;
    default:
        return std::unexpected(make_error(
            EvaluationErrorCode::UnsupportedExpression,
            "Unsupported floating-point operator"
        ));
    }
    return common::Value {common::ValueData {result}};
}

// 执行同类型值的有序比较
template <typename T>
[[nodiscard]]
bool ordered_compare(T left, common::BinaryOperator op, T right)
{
    switch (op) {
    case common::BinaryOperator::LessThan:
        return left < right;
    case common::BinaryOperator::LessThanOrEqual:
        return left <= right;
    case common::BinaryOperator::GreaterThan:
        return left > right;
    case common::BinaryOperator::GreaterThanOrEqual:
        return left >= right;
    default:
        return false;
    }
}

// 按 SQL LIKE 通配规则匹配字符串
// 百分号匹配任意长度字节序列，下划线匹配单个字节；使用滚动动态规划限制额外空间
[[nodiscard]]
bool like_matches(const std::string & value, const std::string & pattern)
{
    std::vector<bool> previous(pattern.size() + 1, false);
    std::vector<bool> current(pattern.size() + 1, false);
    previous[0] = true;
    for (std::size_t pattern_index = 1; pattern_index <= pattern.size(); ++pattern_index) {
        if (pattern[pattern_index - 1] == '%') {
            previous[pattern_index] = previous[pattern_index - 1];
        }
    }

    for (const auto character : value) {
        current[0] = false;
        for (std::size_t pattern_index = 1; pattern_index <= pattern.size(); ++pattern_index) {
            const auto pattern_character = pattern[pattern_index - 1];
            if (pattern_character == '%') {
                current[pattern_index] = current[pattern_index - 1] || previous[pattern_index];
            } else {
                current[pattern_index] =
                    previous[pattern_index - 1] &&
                    (pattern_character == '_' || pattern_character == character);
            }
        }
        std::swap(previous, current);
    }
    return previous[pattern.size()];
}

} // namespace

std::expected<common::Value, EvaluationError> evaluate_unary_value(
    common::UnaryOperator op,
    const common::Value & operand,
    const common::LogicalType & result_type
)
{
    if (operand.is_null()) {
        return common::Value::null();
    }

    if (op == common::UnaryOperator::Not) {
        const auto * value = std::get_if<bool>(&operand.data());
        if (value == nullptr) {
            return std::unexpected(
                make_error(EvaluationErrorCode::InvalidType, "NOT expects BOOLEAN")
            );
        }
        return common::Value {common::ValueData {!*value}};
    }

    switch (result_type.id) {
    case common::LogicalTypeId::Integer: {
        const auto * value = std::get_if<std::int32_t>(&operand.data());
        if (value == nullptr) {
            break;
        }
        if (*value == std::numeric_limits<std::int32_t>::min()) {
            return std::unexpected(
                make_error(EvaluationErrorCode::NumericOverflow, "Integer negation overflow")
            );
        }
        return common::Value {common::ValueData {static_cast<std::int32_t>(-*value)}};
    }
    case common::LogicalTypeId::BigInt: {
        const auto * value = std::get_if<std::int64_t>(&operand.data());
        if (value == nullptr) {
            break;
        }
        if (*value == std::numeric_limits<std::int64_t>::min()) {
            return std::unexpected(
                make_error(EvaluationErrorCode::NumericOverflow, "BigInt negation overflow")
            );
        }
        return common::Value {common::ValueData {static_cast<std::int64_t>(-*value)}};
    }
    case common::LogicalTypeId::Float: {
        const auto * value = std::get_if<float>(&operand.data());
        if (value != nullptr) {
            return common::Value {common::ValueData {-*value}};
        }
        break;
    }
    case common::LogicalTypeId::Double: {
        const auto * value = std::get_if<double>(&operand.data());
        if (value != nullptr) {
            return common::Value {common::ValueData {-*value}};
        }
        break;
    }
    default:
        break;
    }

    return std::unexpected(make_error(
        EvaluationErrorCode::InvalidType,
        "Negation expects a value matching its numeric result type"
    ));
}

std::expected<common::Value, EvaluationError> evaluate_binary_values(
    common::BinaryOperator op,
    const common::Value & left,
    const common::Value & right,
    const common::LogicalType & result_type
)
{
    if (op == common::BinaryOperator::And) {
        return logical_and(left, right);
    }
    if (op == common::BinaryOperator::Or) {
        return logical_or(left, right);
    }
    if (op == common::BinaryOperator::Equal || op == common::BinaryOperator::NotEqual ||
        op == common::BinaryOperator::LessThan || op == common::BinaryOperator::LessThanOrEqual ||
        op == common::BinaryOperator::GreaterThan ||
        op == common::BinaryOperator::GreaterThanOrEqual) {
        return compare_values(left, op, right);
    }
    if (left.is_null() || right.is_null()) {
        return common::Value::null();
    }

    switch (result_type.id) {
    case common::LogicalTypeId::Integer: {
        const auto * left_value = std::get_if<std::int32_t>(&left.data());
        const auto * right_value = std::get_if<std::int32_t>(&right.data());
        if (left_value != nullptr && right_value != nullptr) {
            return evaluate_integer_binary(op, *left_value, *right_value);
        }
        break;
    }
    case common::LogicalTypeId::BigInt: {
        const auto * left_value = std::get_if<std::int64_t>(&left.data());
        const auto * right_value = std::get_if<std::int64_t>(&right.data());
        if (left_value != nullptr && right_value != nullptr) {
            return evaluate_integer_binary(op, *left_value, *right_value);
        }
        break;
    }
    case common::LogicalTypeId::Float: {
        const auto * left_value = std::get_if<float>(&left.data());
        const auto * right_value = std::get_if<float>(&right.data());
        if (left_value != nullptr && right_value != nullptr) {
            return evaluate_floating_binary(op, *left_value, *right_value);
        }
        break;
    }
    case common::LogicalTypeId::Double: {
        const auto * left_value = std::get_if<double>(&left.data());
        const auto * right_value = std::get_if<double>(&right.data());
        if (left_value != nullptr && right_value != nullptr) {
            return evaluate_floating_binary(op, *left_value, *right_value);
        }
        break;
    }
    default:
        break;
    }

    return std::unexpected(make_error(
        EvaluationErrorCode::InvalidType,
        "Arithmetic operands do not match their bound result type"
    ));
}

std::expected<common::Value, EvaluationError>
compare_values(const common::Value & left, common::BinaryOperator op, const common::Value & right)
{
    if (left.is_null() || right.is_null()) {
        return common::Value::null();
    }
    if (left.data().index() != right.data().index()) {
        return std::unexpected(make_error(
            EvaluationErrorCode::InvalidType,
            "Comparison operands have different runtime types"
        ));
    }

    return std::visit(
        [op](const auto & left_value, const auto & right_value)
            -> std::expected<common::Value, EvaluationError> {
            using Left = std::decay_t<decltype(left_value)>;
            using Right = std::decay_t<decltype(right_value)>;
            if constexpr (!std::is_same_v<Left, Right> || std::is_same_v<Left, common::NullValue>) {
                return std::unexpected(
                    make_error(EvaluationErrorCode::InvalidType, "Invalid comparison values")
                );
            } else {
                if (op == common::BinaryOperator::Equal || op == common::BinaryOperator::NotEqual) {
                    const auto equal = left_value == right_value;
                    return common::Value {
                        common::ValueData {op == common::BinaryOperator::Equal ? equal : !equal}
                    };
                }
                if constexpr (std::is_arithmetic_v<Left> || std::is_same_v<Left, std::string>) {
                    return common::Value {
                        common::ValueData {ordered_compare(left_value, op, right_value)}
                    };
                }
                return std::unexpected(make_error(
                    EvaluationErrorCode::InvalidType,
                    "Values do not support ordered comparison"
                ));
            }
        },
        left.data(),
        right.data()
    );
}

std::expected<common::Value, EvaluationError>
logical_and(const common::Value & left, const common::Value & right)
{
    auto left_value = boolean_value(left);
    if (!left_value.has_value()) {
        return std::unexpected(std::move(left_value.error()));
    }

    auto right_value = boolean_value(right);
    if (!right_value.has_value()) {
        return std::unexpected(std::move(right_value.error()));
    }

    if ((left_value->has_value() && !left_value->value()) ||
        (right_value->has_value() && !right_value->value())) {
        return common::Value {common::ValueData {false}};
    }
    if (!left_value->has_value() || !right_value->has_value()) {
        return common::Value::null();
    }
    return common::Value {common::ValueData {true}};
}

std::expected<common::Value, EvaluationError>
logical_or(const common::Value & left, const common::Value & right)
{
    auto left_value = boolean_value(left);
    if (!left_value.has_value()) {
        return std::unexpected(std::move(left_value.error()));
    }

    auto right_value = boolean_value(right);
    if (!right_value.has_value()) {
        return std::unexpected(std::move(right_value.error()));
    }

    if ((left_value->has_value() && left_value->value()) ||
        (right_value->has_value() && right_value->value())) {
        return common::Value {common::ValueData {true}};
    }
    if (!left_value->has_value() || !right_value->has_value()) {
        return common::Value::null();
    }
    return common::Value {common::ValueData {false}};
}

std::expected<common::Value, EvaluationError>
cast_value(const common::Value & value, const common::LogicalType & target_type)
{
    if (value.is_null()) {
        return common::Value::null();
    }
    if (value.matches_type(target_type)) {
        if (target_type.id == common::LogicalTypeId::Vector && target_type.parameter.has_value() &&
            std::get<common::VectorValue>(value.data()).size() != target_type.parameter.value()) {
            return std::unexpected(make_error(
                EvaluationErrorCode::CastFailed,
                "Vector dimension does not match target type"
            ));
        }
        return value;
    }

    return std::visit(
        [&target_type](const auto & data) -> std::expected<common::Value, EvaluationError> {
            using T = std::decay_t<decltype(data)>;
            if constexpr (std::is_same_v<T, std::int32_t>) {
                if (target_type.id == common::LogicalTypeId::BigInt) {
                    return common::Value {common::ValueData {static_cast<std::int64_t>(data)}};
                }
                if (target_type.id == common::LogicalTypeId::Float) {
                    return common::Value {common::ValueData {static_cast<float>(data)}};
                }
                if (target_type.id == common::LogicalTypeId::Double) {
                    return common::Value {common::ValueData {static_cast<double>(data)}};
                }
            } else if constexpr (std::is_same_v<T, std::int64_t>) {
                if (target_type.id == common::LogicalTypeId::Float) {
                    return common::Value {common::ValueData {static_cast<float>(data)}};
                }
                if (target_type.id == common::LogicalTypeId::Double) {
                    return common::Value {common::ValueData {static_cast<double>(data)}};
                }
            } else if constexpr (std::is_same_v<T, float>) {
                if (target_type.id == common::LogicalTypeId::Double) {
                    return common::Value {common::ValueData {static_cast<double>(data)}};
                }
            }
            return std::unexpected(
                make_error(EvaluationErrorCode::CastFailed, "Unsupported implicit cast")
            );
        },
        value.data()
    );
}

std::expected<common::Value, EvaluationError>
evaluate_like_values(const common::Value & value, const common::Value & pattern)
{
    if (value.is_null() || pattern.is_null()) {
        return common::Value::null();
    }

    const auto * string_value = std::get_if<std::string>(&value.data());
    const auto * string_pattern = std::get_if<std::string>(&pattern.data());
    if (string_value == nullptr || string_pattern == nullptr) {
        return std::unexpected(
            make_error(EvaluationErrorCode::InvalidType, "LIKE expects VARCHAR operands")
        );
    }
    return common::Value {common::ValueData {like_matches(*string_value, *string_pattern)}};
}

std::expected<bool, EvaluationError> filter_matches(const common::Value & value)
{
    if (value.is_null()) {
        return false;
    }
    if (const auto * boolean = std::get_if<bool>(&value.data())) {
        return *boolean;
    }
    return std::unexpected(make_error(
        EvaluationErrorCode::InvalidType,
        "Filter predicate must produce BOOLEAN or NULL"
    ));
}

} // namespace litedb::core::evaluator
