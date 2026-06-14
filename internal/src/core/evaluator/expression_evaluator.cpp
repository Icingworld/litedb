#include "core/evaluator/expression_evaluator.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "core/binder/bound/expression/bound_between_expression.hpp"
#include "core/binder/bound/expression/bound_binary_expression.hpp"
#include "core/binder/bound/expression/bound_cast_expression.hpp"
#include "core/binder/bound/expression/bound_column_ref_expression.hpp"
#include "core/binder/bound/expression/bound_function_expression.hpp"
#include "core/binder/bound/expression/bound_in_expression.hpp"
#include "core/binder/bound/expression/bound_like_expression.hpp"
#include "core/binder/bound/expression/bound_literal_expression.hpp"
#include "core/binder/bound/expression/bound_unary_expression.hpp"
#include "core/binder/bound/expression/bound_vector_expression.hpp"
#include "core/function/function_error.hpp"
#include "core/parser/token.hpp"

namespace litedb::core::evaluator
{

namespace
{

using binder::bound::BoundBetweenExpression;
using binder::bound::BoundBinaryExpression;
using binder::bound::BoundCastExpression;
using binder::bound::BoundColumnRefExpression;
using binder::bound::BoundExpression;
using binder::bound::BoundExpressionKind;
using binder::bound::BoundFunctionExpression;
using binder::bound::BoundInExpression;
using binder::bound::BoundLikeExpression;
using binder::bound::BoundLiteralExpression;
using binder::bound::BoundUnaryExpression;
using binder::bound::BoundVectorExpression;
using common::LogicalType;
using common::LogicalTypeId;
using parser::TokenType;

template <typename T>
constexpr bool always_false_v = false;

EvaluationError make_error(
    EvaluationErrorCode code,
    parser::ast::AstNodeLocation location,
    std::string message
)
{
    return EvaluationError {code, location, std::move(message)};
}

[[nodiscard]]
EvaluationError from_function_error(function::FunctionError error)
{
    return EvaluationError {
        .code = error.code == function::FunctionErrorCode::InvalidType
            ? EvaluationErrorCode::InvalidType
            : EvaluationErrorCode::UnsupportedExpression,
        .location = error.location,
        .message = std::move(error.message),
    };
}

/**
 * @brief 判断类型是否为数值类型
 * @param type 类型
 * @return 是否为数值类型
 */
[[nodiscard]]
bool is_numeric_type(LogicalTypeId type) noexcept
{
    return type == LogicalTypeId::Integer || type == LogicalTypeId::BigInt || type == LogicalTypeId::Float
        || type == LogicalTypeId::Double;
}

/**
 * @brief 判断值是否为数值
 * @param value 值
 * @return 是否为数值
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
 * @brief 将值转换为 double
 * @param value 值
 * @param location 位置
 * @return 转换结果
 */
[[nodiscard]]
std::expected<double, EvaluationError> as_double(
    const schema::Value & value,
    parser::ast::AstNodeLocation location
)
{
    return std::visit(
        [location](const auto & data) -> std::expected<double, EvaluationError> {
            using T = std::decay_t<decltype(data)>;
            if constexpr (std::is_same_v<T, std::int32_t> || std::is_same_v<T, std::int64_t>
                || std::is_same_v<T, float> || std::is_same_v<T, double>) {
                return static_cast<double>(data);
            } else {
                return std::unexpected(make_error(EvaluationErrorCode::InvalidType, location, "Expected numeric value"));
            }
        },
        value.data()
    );
}

/**
 * @brief 将值转换为 int64_t
 * @param value 值
 * @param location 位置
 * @return 转换结果
 */
[[nodiscard]]
std::expected<std::int64_t, EvaluationError> as_int64(
    const schema::Value & value,
    parser::ast::AstNodeLocation location
)
{
    return std::visit(
        [location](const auto & data) -> std::expected<std::int64_t, EvaluationError> {
            using T = std::decay_t<decltype(data)>;
            if constexpr (std::is_same_v<T, std::int32_t> || std::is_same_v<T, std::int64_t>) {
                return static_cast<std::int64_t>(data);
            } else {
                return std::unexpected(make_error(EvaluationErrorCode::InvalidType, location, "Expected integer value"));
            }
        },
        value.data()
    );
}

/**
 * @brief 将值转换为可选布尔值
 * @param value 值
 * @return 转换结果
 */
[[nodiscard]]
std::optional<bool> as_optional_bool(const schema::Value & value)
{
    if (value.is_null()) {
        return std::nullopt;
    }
    if (const auto * boolean = std::get_if<bool>(&value.data())) {
        return *boolean;
    }
    return std::nullopt;
}

/**
 * @brief 要求值为布尔值
 * @param value 值
 * @param location 位置
 * @return 转换结果
 */
[[nodiscard]]
std::expected<bool, EvaluationError> require_bool(
    const schema::Value & value,
    parser::ast::AstNodeLocation location
)
{
    if (value.is_null()) {
        return false;
    }
    if (const auto * boolean = std::get_if<bool>(&value.data())) {
        return *boolean;
    }
    return std::unexpected(make_error(EvaluationErrorCode::InvalidType, location, "Expected boolean value"));
}

/**
 * @brief 解析字面量
 * @tparam T 类型
 * @param value 值
 * @param location 位置
 * @return 解析结果
 */
template <typename T>
[[nodiscard]]
std::expected<T, EvaluationError> parse_literal(
    const std::string & value,
    parser::ast::AstNodeLocation location
)
{
    try {
        std::size_t parsed = 0;
        if constexpr (std::is_same_v<T, bool>) {
            if (value == "true" || value == "TRUE") {
                return true;
            }
            if (value == "false" || value == "FALSE") {
                return false;
            }
        } else if constexpr (std::is_same_v<T, std::int32_t>) {
            const auto result = std::stol(value, &parsed);
            if (parsed == value.size()
                && result >= std::numeric_limits<std::int32_t>::min()
                && result <= std::numeric_limits<std::int32_t>::max()) {
                return static_cast<std::int32_t>(result);
            }
        } else if constexpr (std::is_same_v<T, std::int64_t>) {
            const auto result = std::stoll(value, &parsed);
            if (parsed == value.size()) {
                return static_cast<std::int64_t>(result);
            }
        } else if constexpr (std::is_same_v<T, float>) {
            const auto result = std::stof(value, &parsed);
            if (parsed == value.size()) {
                return result;
            }
        } else if constexpr (std::is_same_v<T, double>) {
            const auto result = std::stod(value, &parsed);
            if (parsed == value.size()) {
                return result;
            }
        } else {
            static_assert(always_false_v<T>);
        }
    } catch (const std::exception &) {
    }

    return std::unexpected(make_error(EvaluationErrorCode::InvalidLiteral, location, "Invalid literal: " + value));
}

/**
 * @brief 解析字面量值
 * @param expression 表达式
 * @return 解析结果
 */
[[nodiscard]]
std::expected<schema::Value, EvaluationError> parse_literal_value(const BoundLiteralExpression & expression)
{
    const auto & value = expression.value();
    const auto location = expression.location();

    switch (expression.type().id) {
    case LogicalTypeId::Boolean: {
        auto parsed = parse_literal<bool>(value, location);
        if (!parsed.has_value()) {
            return std::unexpected(std::move(parsed.error()));
        }
        return schema::Value {*parsed};
    }
    case LogicalTypeId::Integer: {
        auto parsed = parse_literal<std::int32_t>(value, location);
        if (!parsed.has_value()) {
            return std::unexpected(std::move(parsed.error()));
        }
        return schema::Value {*parsed};
    }
    case LogicalTypeId::BigInt: {
        auto parsed = parse_literal<std::int64_t>(value, location);
        if (!parsed.has_value()) {
            return std::unexpected(std::move(parsed.error()));
        }
        return schema::Value {*parsed};
    }
    case LogicalTypeId::Float: {
        auto parsed = parse_literal<float>(value, location);
        if (!parsed.has_value()) {
            return std::unexpected(std::move(parsed.error()));
        }
        return schema::Value {*parsed};
    }
    case LogicalTypeId::Double: {
        auto parsed = parse_literal<double>(value, location);
        if (!parsed.has_value()) {
            return std::unexpected(std::move(parsed.error()));
        }
        return schema::Value {*parsed};
    }
    case LogicalTypeId::Varchar:
        return schema::Value {value};
    case LogicalTypeId::Null:
        return schema::Value::null();
    case LogicalTypeId::Vector:
        return std::unexpected(make_error(EvaluationErrorCode::InvalidLiteral, location, "Vector literal must use vector expression"));
    }

    return std::unexpected(make_error(EvaluationErrorCode::InvalidLiteral, location, "Unsupported literal type"));
}

/**
 * @brief 判断两个值是否相等
 * @param left 左值
 * @param right 右值
 * @return 是否相等
 */
[[nodiscard]]
bool values_equal(const schema::Value & left, const schema::Value & right)
{
    if (left.data().index() == right.data().index()) {
        return left.data() == right.data();
    }
    if (is_numeric_value(left) && is_numeric_value(right)) {
        const auto left_number = as_double(left, {0, 0});
        const auto right_number = as_double(right, {0, 0});
        return left_number.has_value() && right_number.has_value() && *left_number == *right_number;
    }
    return false;
}

/**
 * @brief 比较两个值
 * @param left 左值
 * @param op 操作符
 * @param right 右值
 * @param location 位置
 * @return 比较结果
 */
[[nodiscard]]
std::expected<schema::Value, EvaluationError> compare_values(
    const schema::Value & left,
    TokenType op,
    const schema::Value & right,
    parser::ast::AstNodeLocation location
)
{
    if (left.is_null() || right.is_null()) {
        return schema::Value::null();
    }

    if (op == TokenType::Equal || op == TokenType::NotEqual) {
        const auto equal = values_equal(left, right);
        return schema::Value {op == TokenType::Equal ? equal : !equal};
    }

    if (is_numeric_value(left) && is_numeric_value(right)) {
        auto left_number = as_double(left, location);
        auto right_number = as_double(right, location);
        if (!left_number.has_value()) {
            return std::unexpected(std::move(left_number.error()));
        }
        if (!right_number.has_value()) {
            return std::unexpected(std::move(right_number.error()));
        }

        switch (op) {
        case TokenType::LessThan:
            return schema::Value {*left_number < *right_number};
        case TokenType::LessEqual:
            return schema::Value {*left_number <= *right_number};
        case TokenType::GreaterThan:
            return schema::Value {*left_number > *right_number};
        case TokenType::GreaterEqual:
            return schema::Value {*left_number >= *right_number};
        default:
            break;
        }
    }

    if (const auto * left_string = std::get_if<std::string>(&left.data())) {
        if (const auto * right_string = std::get_if<std::string>(&right.data())) {
            switch (op) {
            case TokenType::LessThan:
                return schema::Value {*left_string < *right_string};
            case TokenType::LessEqual:
                return schema::Value {*left_string <= *right_string};
            case TokenType::GreaterThan:
                return schema::Value {*left_string > *right_string};
            case TokenType::GreaterEqual:
                return schema::Value {*left_string >= *right_string};
            default:
                break;
            }
        }
    }

    return std::unexpected(make_error(EvaluationErrorCode::InvalidType, location, "Values are not comparable"));
}

/**
 * @brief 计算数值
 * @param left 左值
 * @param op 操作符
 * @param right 右值
 * @param result_type 结果类型
 * @param location 位置
 * @return 计算结果
 */
[[nodiscard]]
std::expected<schema::Value, EvaluationError> calculate_numeric(
    const schema::Value & left,
    TokenType op,
    const schema::Value & right,
    const LogicalType & result_type,
    parser::ast::AstNodeLocation location
)
{
    if (left.is_null() || right.is_null()) {
        return schema::Value::null();
    }

    if (result_type.id == LogicalTypeId::Integer || result_type.id == LogicalTypeId::BigInt) {
        auto left_number = as_int64(left, location);
        auto right_number = as_int64(right, location);
        if (!left_number.has_value()) {
            return std::unexpected(std::move(left_number.error()));
        }
        if (!right_number.has_value()) {
            return std::unexpected(std::move(right_number.error()));
        }

        if ((op == TokenType::Slash || op == TokenType::Modulo) && *right_number == 0) {
            return std::unexpected(make_error(EvaluationErrorCode::DivisionByZero, location, "Division by zero"));
        }

        std::int64_t result = 0;
        switch (op) {
        case TokenType::Plus:
            result = *left_number + *right_number;
            break;
        case TokenType::Minus:
            result = *left_number - *right_number;
            break;
        case TokenType::Star:
            result = *left_number * *right_number;
            break;
        case TokenType::Slash:
            result = *left_number / *right_number;
            break;
        case TokenType::Modulo:
            result = *left_number % *right_number;
            break;
        default:
            return std::unexpected(make_error(EvaluationErrorCode::InvalidType, location, "Unsupported arithmetic operator"));
        }

        if (result_type.id == LogicalTypeId::Integer) {
            return schema::Value {static_cast<std::int32_t>(result)};
        }
        return schema::Value {result};
    }

    auto left_number = as_double(left, location);
    auto right_number = as_double(right, location);
    if (!left_number.has_value()) {
        return std::unexpected(std::move(left_number.error()));
    }
    if (!right_number.has_value()) {
        return std::unexpected(std::move(right_number.error()));
    }

    if ((op == TokenType::Slash || op == TokenType::Modulo) && *right_number == 0.0) {
        return std::unexpected(make_error(EvaluationErrorCode::DivisionByZero, location, "Division by zero"));
    }

    double result = 0.0;
    switch (op) {
    case TokenType::Plus:
        result = *left_number + *right_number;
        break;
    case TokenType::Minus:
        result = *left_number - *right_number;
        break;
    case TokenType::Star:
        result = *left_number * *right_number;
        break;
    case TokenType::Slash:
        result = *left_number / *right_number;
        break;
    case TokenType::Modulo:
        result = std::fmod(*left_number, *right_number);
        break;
    default:
        return std::unexpected(make_error(EvaluationErrorCode::InvalidType, location, "Unsupported arithmetic operator"));
    }

    if (result_type.id == LogicalTypeId::Float) {
        return schema::Value {static_cast<float>(result)};
    }
    return schema::Value {result};
}

/**
 * @brief 三值逻辑与
 * @param left 左值
 * @param right 右值
 * @return 逻辑与结果
 */
[[nodiscard]]
schema::Value three_value_and(const schema::Value & left, const schema::Value & right)
{
    const auto left_bool = as_optional_bool(left);
    const auto right_bool = as_optional_bool(right);
    if ((left_bool.has_value() && !*left_bool) || (right_bool.has_value() && !*right_bool)) {
        return schema::Value {false};
    }
    if (!left_bool.has_value() || !right_bool.has_value()) {
        return schema::Value::null();
    }
    return schema::Value {*left_bool && *right_bool};
}

/**
 * @brief 三值逻辑或
 * @param left 左值
 * @param right 右值
 * @return 逻辑或结果
 */
[[nodiscard]]
schema::Value three_value_or(const schema::Value & left, const schema::Value & right)
{
    const auto left_bool = as_optional_bool(left);
    const auto right_bool = as_optional_bool(right);
    if ((left_bool.has_value() && *left_bool) || (right_bool.has_value() && *right_bool)) {
        return schema::Value {true};
    }
    if (!left_bool.has_value() || !right_bool.has_value()) {
        return schema::Value::null();
    }
    return schema::Value {*left_bool || *right_bool};
}

/**
 * @brief 判断 LIKE 表达式是否匹配
 * @param value 值
 * @param pattern 模式
 * @return 是否匹配
 */
[[nodiscard]]
bool like_matches(const std::string & value, const std::string & pattern)
{
    std::vector<std::vector<bool>> matches(value.size() + 1, std::vector<bool>(pattern.size() + 1, false));
    matches[0][0] = true;

    for (std::size_t pattern_index = 1; pattern_index <= pattern.size(); ++pattern_index) {
        if (pattern[pattern_index - 1] == '%') {
            matches[0][pattern_index] = matches[0][pattern_index - 1];
        }
    }

    for (std::size_t value_index = 1; value_index <= value.size(); ++value_index) {
        for (std::size_t pattern_index = 1; pattern_index <= pattern.size(); ++pattern_index) {
            const auto pattern_char = pattern[pattern_index - 1];
            if (pattern_char == '%') {
                matches[value_index][pattern_index] = matches[value_index][pattern_index - 1]
                    || matches[value_index - 1][pattern_index];
            } else if (pattern_char == '_' || pattern_char == value[value_index - 1]) {
                matches[value_index][pattern_index] = matches[value_index - 1][pattern_index - 1];
            }
        }
    }

    return matches[value.size()][pattern.size()];
}

/**
 * @brief 转换值
 * @param value 值
 * @param target_type 目标类型
 * @param location 位置
 * @return 转换结果
 */
[[nodiscard]]
std::expected<schema::Value, EvaluationError> cast_value(
    const schema::Value & value,
    const LogicalType & target_type,
    parser::ast::AstNodeLocation location
)
{
    if (value.is_null()) {
        return schema::Value::null();
    }

    switch (target_type.id) {
    case LogicalTypeId::Boolean:
        if (const auto * boolean = std::get_if<bool>(&value.data())) {
            return schema::Value {*boolean};
        }
        break;
    case LogicalTypeId::Integer:
        if (is_numeric_value(value)) {
            auto number = as_double(value, location);
            if (!number.has_value()) {
                return std::unexpected(std::move(number.error()));
            }
            return schema::Value {static_cast<std::int32_t>(*number)};
        }
        break;
    case LogicalTypeId::BigInt:
        if (is_numeric_value(value)) {
            auto number = as_double(value, location);
            if (!number.has_value()) {
                return std::unexpected(std::move(number.error()));
            }
            return schema::Value {static_cast<std::int64_t>(*number)};
        }
        break;
    case LogicalTypeId::Float:
        if (is_numeric_value(value)) {
            auto number = as_double(value, location);
            if (!number.has_value()) {
                return std::unexpected(std::move(number.error()));
            }
            return schema::Value {static_cast<float>(*number)};
        }
        break;
    case LogicalTypeId::Double:
        if (is_numeric_value(value)) {
            auto number = as_double(value, location);
            if (!number.has_value()) {
                return std::unexpected(std::move(number.error()));
            }
            return schema::Value {*number};
        }
        break;
    case LogicalTypeId::Varchar:
        return std::visit(
            [location](const auto & data) -> std::expected<schema::Value, EvaluationError> {
                using T = std::decay_t<decltype(data)>;
                if constexpr (std::is_same_v<T, bool>) {
                    return schema::Value {data ? std::string {"true"} : std::string {"false"}};
                } else if constexpr (std::is_same_v<T, std::int32_t> || std::is_same_v<T, std::int64_t>
                    || std::is_same_v<T, float> || std::is_same_v<T, double>) {
                    return schema::Value {std::to_string(data)};
                } else if constexpr (std::is_same_v<T, std::string>) {
                    return schema::Value {data};
                } else {
                    return std::unexpected(make_error(EvaluationErrorCode::CastFailed, location, "Cannot cast value to string"));
                }
            },
            value.data()
        );
    case LogicalTypeId::Vector:
        if (std::holds_alternative<schema::VectorValue>(value.data())) {
            return value;
        }
        break;
    case LogicalTypeId::Null:
        return schema::Value::null();
    }

    return std::unexpected(make_error(EvaluationErrorCode::CastFailed, location, "Unsupported cast"));
}

/**
 * @brief 评估工作器
 * @param record 记录
 */
class EvaluationWorker
{
public:
    explicit EvaluationWorker(const schema::Record & record)
        : record_(record)
    {
    }

public:
    /**
     * @brief 评估表达式
     * @param expression 表达式
     * @return 评估结果
     */
    [[nodiscard]]
    std::expected<schema::Value, EvaluationError> evaluate(const BoundExpression & expression)
    {
        switch (expression.kind()) {
        case BoundExpressionKind::Literal:
            return parse_literal_value(static_cast<const BoundLiteralExpression &>(expression));
        case BoundExpressionKind::Null:
            return schema::Value::null();
        case BoundExpressionKind::ColumnRef:
            return eval_column_ref(static_cast<const BoundColumnRefExpression &>(expression));
        case BoundExpressionKind::Unary:
            return eval_unary(static_cast<const BoundUnaryExpression &>(expression));
        case BoundExpressionKind::Binary:
            return eval_binary(static_cast<const BoundBinaryExpression &>(expression));
        case BoundExpressionKind::Vector:
            return eval_vector(static_cast<const BoundVectorExpression &>(expression));
        case BoundExpressionKind::Function:
            return eval_function(static_cast<const BoundFunctionExpression &>(expression));
        case BoundExpressionKind::In:
            return eval_in(static_cast<const BoundInExpression &>(expression));
        case BoundExpressionKind::Between:
            return eval_between(static_cast<const BoundBetweenExpression &>(expression));
        case BoundExpressionKind::Like:
            return eval_like(static_cast<const BoundLikeExpression &>(expression));
        case BoundExpressionKind::Cast:
            return eval_cast(static_cast<const BoundCastExpression &>(expression));
        case BoundExpressionKind::Wildcard:
            return std::unexpected(make_error(
                EvaluationErrorCode::UnsupportedExpression,
                expression.location(),
                "Wildcard cannot be evaluated directly"
            ));
        }

        return std::unexpected(make_error(
            EvaluationErrorCode::UnsupportedExpression,
            expression.location(),
            "Unsupported expression"
        ));
    }

    [[nodiscard]]
    std::expected<schema::Value, EvaluationError> eval_function(const BoundFunctionExpression & expression)
    {
        std::vector<schema::Value> arguments;
        arguments.reserve(expression.arguments().size());

        for (const auto & argument : expression.arguments()) {
            auto value = evaluate(*argument);
            if (!value.has_value()) {
                return std::unexpected(std::move(value.error()));
            }
            arguments.push_back(std::move(value.value()));
        }

        auto result = expression.function().evaluate(arguments, function::ScalarFunctionContext {}, expression.location());
        if (!result.has_value()) {
            return std::unexpected(from_function_error(std::move(result.error())));
        }
        return std::move(result.value());
    }

private:
    /**
     * @brief 评估列引用
     * @param expression 表达式
     * @return 评估结果
     */
    [[nodiscard]]
    std::expected<schema::Value, EvaluationError> eval_column_ref(const BoundColumnRefExpression & expression) const
    {
        if (expression.column_id() == 0) {
            return std::unexpected(make_error(
                EvaluationErrorCode::InvalidColumnReference,
                expression.location(),
                "Column id must be positive"
            ));
        }

        const auto index = static_cast<std::size_t>(expression.column_id() - 1);
        if (index >= record_.data.values.size()) {
            return std::unexpected(make_error(
                EvaluationErrorCode::InvalidColumnReference,
                expression.location(),
                "Column reference is out of range: " + expression.column_name()
            ));
        }

        return record_.data.values[index];
    }

    /**
     * @brief 评估一元表达式
     * @param expression 表达式
     * @return 评估结果
     */
    [[nodiscard]]
    std::expected<schema::Value, EvaluationError> eval_unary(const BoundUnaryExpression & expression)
    {
        auto operand = evaluate(expression.operand());
        if (!operand.has_value()) {
            return std::unexpected(std::move(operand.error()));
        }
        if (operand->is_null()) {
            return schema::Value::null();
        }

        switch (expression.op()) {
        case TokenType::Not: {
            if (const auto * boolean = std::get_if<bool>(&operand->data())) {
                return schema::Value {!*boolean};
            }
            return std::unexpected(make_error(EvaluationErrorCode::InvalidType, expression.location(), "NOT expects boolean"));
        }
        case TokenType::Plus:
            if (is_numeric_value(*operand)) {
                return *operand;
            }
            break;
        case TokenType::Minus:
            if (expression.type().id == LogicalTypeId::Integer || expression.type().id == LogicalTypeId::BigInt) {
                auto number = as_int64(*operand, expression.location());
                if (!number.has_value()) {
                    return std::unexpected(std::move(number.error()));
                }
                if (expression.type().id == LogicalTypeId::Integer) {
                    return schema::Value {static_cast<std::int32_t>(-*number)};
                }
                return schema::Value {-*number};
            }
            if (is_numeric_value(*operand)) {
                auto number = as_double(*operand, expression.location());
                if (!number.has_value()) {
                    return std::unexpected(std::move(number.error()));
                }
                if (expression.type().id == LogicalTypeId::Float) {
                    return schema::Value {static_cast<float>(-*number)};
                }
                return schema::Value {-*number};
            }
            break;
        default:
            break;
        }

        return std::unexpected(make_error(EvaluationErrorCode::InvalidType, expression.location(), "Invalid unary operand"));
    }

    /**
     * @brief 评估二元表达式
     * @param expression 表达式
     * @return 评估结果
     */
    [[nodiscard]]
    std::expected<schema::Value, EvaluationError> eval_binary(const BoundBinaryExpression & expression)
    {
        auto left = evaluate(expression.left());
        if (!left.has_value()) {
            return std::unexpected(std::move(left.error()));
        }
        auto right = evaluate(expression.right());
        if (!right.has_value()) {
            return std::unexpected(std::move(right.error()));
        }

        switch (expression.op()) {
        case TokenType::And:
            return three_value_and(*left, *right);
        case TokenType::Or:
            return three_value_or(*left, *right);
        case TokenType::Equal:
            [[fallthrough]];
        case TokenType::NotEqual:
            [[fallthrough]];
        case TokenType::LessThan:
            [[fallthrough]];
        case TokenType::LessEqual:
            [[fallthrough]];
        case TokenType::GreaterThan:
            [[fallthrough]];
        case TokenType::GreaterEqual:
            return compare_values(*left, expression.op(), *right, expression.location());
        case TokenType::Plus:
            [[fallthrough]];
        case TokenType::Minus:
            [[fallthrough]];
        case TokenType::Star:
            [[fallthrough]];
        case TokenType::Slash:
            [[fallthrough]];
        case TokenType::Modulo:
            if (!is_numeric_type(expression.type().id)) {
                return std::unexpected(make_error(EvaluationErrorCode::InvalidType, expression.location(), "Arithmetic result is not numeric"));
            }
            return calculate_numeric(*left, expression.op(), *right, expression.type(), expression.location());
        default:
            return std::unexpected(make_error(EvaluationErrorCode::UnsupportedExpression, expression.location(), "Unsupported binary operator"));
        }
    }

    /**
     * @brief 评估向量表达式
     * @param expression 表达式
     * @return 评估结果
     */
    [[nodiscard]]
    std::expected<schema::Value, EvaluationError> eval_vector(const BoundVectorExpression & expression)
    {
        schema::VectorValue values;
        values.reserve(expression.elements().size());

        for (const auto & element : expression.elements()) {
            auto value = evaluate(*element);
            if (!value.has_value()) {
                return std::unexpected(std::move(value.error()));
            }
            if (value->is_null()) {
                return schema::Value::null();
            }
            auto number = as_double(*value, element->location());
            if (!number.has_value()) {
                return std::unexpected(std::move(number.error()));
            }
            values.push_back(*number);
        }

        return schema::Value {std::move(values)};
    }

    /**
     * @brief 评估 IN 表达式
     * @param expression 表达式
     * @return 评估结果
     */
    [[nodiscard]]
    std::expected<schema::Value, EvaluationError> eval_in(const BoundInExpression & expression)
    {
        auto target = evaluate(expression.expression());
        if (!target.has_value()) {
            return std::unexpected(std::move(target.error()));
        }
        if (target->is_null()) {
            return schema::Value::null();
        }

        bool saw_null = false;
        for (const auto & candidate_expression : expression.values()) {
            auto candidate = evaluate(*candidate_expression);
            if (!candidate.has_value()) {
                return std::unexpected(std::move(candidate.error()));
            }
            if (candidate->is_null()) {
                saw_null = true;
                continue;
            }
            if (values_equal(*target, *candidate)) {
                return schema::Value {true};
            }
        }

        if (saw_null) {
            return schema::Value::null();
        }
        return schema::Value {false};
    }

    /**
     * @brief 评估 BETWEEN 表达式
     * @param expression 表达式
     * @return 评估结果
     */
    [[nodiscard]]
    std::expected<schema::Value, EvaluationError> eval_between(const BoundBetweenExpression & expression)
    {
        auto target = evaluate(expression.expression());
        if (!target.has_value()) {
            return std::unexpected(std::move(target.error()));
        }
        auto lower = evaluate(expression.lower());
        if (!lower.has_value()) {
            return std::unexpected(std::move(lower.error()));
        }
        auto upper = evaluate(expression.upper());
        if (!upper.has_value()) {
            return std::unexpected(std::move(upper.error()));
        }

        auto lower_result = compare_values(*target, TokenType::GreaterEqual, *lower, expression.location());
        if (!lower_result.has_value()) {
            return std::unexpected(std::move(lower_result.error()));
        }
        auto upper_result = compare_values(*target, TokenType::LessEqual, *upper, expression.location());
        if (!upper_result.has_value()) {
            return std::unexpected(std::move(upper_result.error()));
        }
        return three_value_and(*lower_result, *upper_result);
    }

    /**
     * @brief 评估 LIKE 表达式
     * @param expression 表达式
     * @return 评估结果
     */
    [[nodiscard]]
    std::expected<schema::Value, EvaluationError> eval_like(const BoundLikeExpression & expression)
    {
        auto value = evaluate(expression.expression());
        if (!value.has_value()) {
            return std::unexpected(std::move(value.error()));
        }
        auto pattern = evaluate(expression.pattern());
        if (!pattern.has_value()) {
            return std::unexpected(std::move(pattern.error()));
        }
        if (value->is_null() || pattern->is_null()) {
            return schema::Value::null();
        }

        const auto * value_string = std::get_if<std::string>(&value->data());
        const auto * pattern_string = std::get_if<std::string>(&pattern->data());
        if (value_string == nullptr || pattern_string == nullptr) {
            return std::unexpected(make_error(EvaluationErrorCode::InvalidType, expression.location(), "LIKE expects strings"));
        }

        return schema::Value {like_matches(*value_string, *pattern_string)};
    }

    /**
     * @brief 评估 CAST 表达式
     * @param expression 表达式
     * @return 评估结果
     */
    [[nodiscard]]
    std::expected<schema::Value, EvaluationError> eval_cast(const BoundCastExpression & expression)
    {
        auto value = evaluate(expression.expression());
        if (!value.has_value()) {
            return std::unexpected(std::move(value.error()));
        }
        return cast_value(*value, expression.type(), expression.location());
    }

private:
    const schema::Record & record_;     ///< 记录
};

} // namespace

std::expected<schema::Value, EvaluationError> ExpressionEvaluator::evaluate(
    const binder::bound::BoundExpression & expression,
    const schema::Record & record
) const
{
    EvaluationWorker worker {record};
    return worker.evaluate(expression);
}

std::expected<bool, EvaluationError> ExpressionEvaluator::evaluate_predicate(
    const binder::bound::BoundExpression & expression,
    const schema::Record & record
) const
{
    auto value = evaluate(expression, record);
    if (!value.has_value()) {
        return std::unexpected(std::move(value.error()));
    }
    return require_bool(*value, expression.location());
}

} // namespace litedb::core::evaluator
