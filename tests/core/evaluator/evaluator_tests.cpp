#include "core/binder/bound/expression/bound_between_expression.hpp"
#include "core/binder/bound/expression/bound_binary_expression.hpp"
#include "core/binder/bound/expression/bound_cast_expression.hpp"
#include "core/binder/bound/expression/bound_column_ref_expression.hpp"
#include "core/binder/bound/expression/bound_function_expression.hpp"
#include "core/binder/bound/expression/bound_in_expression.hpp"
#include "core/binder/bound/expression/bound_like_expression.hpp"
#include "core/binder/bound/expression/bound_literal_expression.hpp"
#include "core/binder/bound/expression/bound_null_expression.hpp"
#include "core/binder/bound/expression/bound_unary_expression.hpp"
#include "core/binder/bound/expression/bound_vector_expression.hpp"
#include "core/evaluator/expression_evaluator.hpp"
#include "core/function/builtin/builtin_functions.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{

using namespace litedb::core::binder::bound;
using namespace litedb::core::common;
using namespace litedb::core::evaluator;

void require(bool condition, const char * message)
{
    if (!condition) throw std::runtime_error(message);
}

LogicalType type(LogicalTypeId id, std::optional<std::size_t> parameter = std::nullopt)
{
    return LogicalType {id, parameter};
}

template <typename T>
std::unique_ptr<BoundExpression> literal(LogicalTypeId type_id, T value)
{
    return std::make_unique<BoundLiteralExpression>(
        type(type_id),
        Value {ValueData {std::move(value)}}
    );
}

std::unique_ptr<BoundExpression> null_expr(LogicalTypeId type_id = LogicalTypeId::Null)
{
    return std::make_unique<BoundNullExpression>(type(type_id));
}

std::unique_ptr<BoundExpression> column(
    ColumnId column_id,
    std::size_t ordinal,
    LogicalTypeId type_id,
    std::optional<std::size_t> parameter = std::nullopt
)
{
    return std::make_unique<BoundColumnRefExpression>(
        column_id,
        ordinal,
        type(type_id, parameter)
    );
}

std::vector<Value> row_values()
{
    return {
        Value {ValueData {std::int64_t {42}}},
        Value {ValueData {std::string {"alice"}}},
        Value {ValueData {std::int32_t {18}}},
        Value {ValueData {true}},
        Value::null(),
    };
}

template <typename T>
const T & get_value(const Value & value)
{
    return std::get<T>(value.data());
}

void require_error(
    const std::expected<Value, EvaluationError> & result,
    EvaluationErrorCode code,
    const char * message
)
{
    require(!result.has_value(), message);
    require(result.error().is(code), message);
}

void require_boolean(
    const std::expected<Value, EvaluationError> & result,
    std::optional<bool> expected,
    const char * message
)
{
    require(result.has_value(), message);
    if (!expected.has_value()) {
        require(result->is_null(), message);
        return;
    }
    require(!result->is_null(), message);
    require(get_value<bool>(*result) == *expected, message);
}

std::unique_ptr<BoundExpression> boolean_expression(std::optional<bool> value)
{
    if (!value.has_value()) return null_expr(LogicalTypeId::Boolean);
    return literal(LogicalTypeId::Boolean, *value);
}

BoundBinaryExpression binary(
    std::unique_ptr<BoundExpression> left,
    BinaryOperator op,
    std::unique_ptr<BoundExpression> right,
    LogicalTypeId result_type
)
{
    return BoundBinaryExpression {
        std::move(left),
        op,
        std::move(right),
        type(result_type),
    };
}

void test_literals_vector_and_ordinal_column_ref()
{
    auto values = row_values();
    ExpressionEvaluator evaluator {EvaluationContext {.input_values = values}};

    auto integer = evaluator.evaluate(*literal(LogicalTypeId::Integer, std::int32_t {123}));
    require(integer.has_value() && get_value<std::int32_t>(*integer) == 123, "integer literal failed");

    auto name = evaluator.evaluate(*column(9000, 1, LogicalTypeId::Varchar));
    require(name.has_value() && get_value<std::string>(*name) == "alice", "column must use ordinal, not id");

    auto null_column = evaluator.evaluate(*column(9001, 4, LogicalTypeId::Integer));
    require(null_column.has_value() && null_column->is_null(), "NULL column must match any bound nullable type");

    std::vector<std::unique_ptr<BoundExpression>> elements;
    elements.push_back(literal(LogicalTypeId::Double, 0.1));
    elements.push_back(literal(LogicalTypeId::Double, 0.2));
    BoundVectorExpression vector {std::move(elements)};
    auto vector_value = evaluator.evaluate(vector);
    require(vector_value.has_value(), "vector evaluation failed");
    require(get_value<VectorValue>(*vector_value) == VectorValue({0.1, 0.2}), "vector value mismatch");
}

void test_exact_numeric_semantics_and_overflow()
{
    auto add = BoundBinaryExpression {
        literal(LogicalTypeId::Integer, std::int32_t {2}),
        BinaryOperator::Add,
        literal(LogicalTypeId::Integer, std::int32_t {3}),
        type(LogicalTypeId::Integer),
    };
    auto add_result = ExpressionEvaluator::evaluate_constant(add);
    require(add_result.has_value() && get_value<std::int32_t>(*add_result) == 5, "integer add failed");

    auto exact = BoundBinaryExpression {
        literal(LogicalTypeId::BigInt, std::int64_t {9007199254740993LL}),
        BinaryOperator::GreaterThan,
        literal(LogicalTypeId::BigInt, std::int64_t {9007199254740992LL}),
        type(LogicalTypeId::Boolean),
    };
    auto exact_result = ExpressionEvaluator::evaluate_constant(exact);
    require(exact_result.has_value() && get_value<bool>(*exact_result), "BIGINT comparison lost precision");

    auto overflow = BoundBinaryExpression {
        literal(LogicalTypeId::Integer, std::numeric_limits<std::int32_t>::max()),
        BinaryOperator::Add,
        literal(LogicalTypeId::Integer, std::int32_t {1}),
        type(LogicalTypeId::Integer),
    };
    auto overflow_result = ExpressionEvaluator::evaluate_constant(overflow);
    require(!overflow_result.has_value(), "integer overflow should fail");
    require(overflow_result.error().is(EvaluationErrorCode::NumericOverflow), "overflow error mismatch");

    auto divide = BoundBinaryExpression {
        literal(LogicalTypeId::BigInt, std::int64_t {1}),
        BinaryOperator::Divide,
        literal(LogicalTypeId::BigInt, std::int64_t {0}),
        type(LogicalTypeId::BigInt),
    };
    auto divide_result = ExpressionEvaluator::evaluate_constant(divide);
    require(!divide_result.has_value(), "division by zero should fail");
    require(divide_result.error().is(EvaluationErrorCode::DivisionByZero), "division error mismatch");
}

void test_three_value_logic_and_filter_semantics()
{
    auto null_and_false = BoundBinaryExpression {
        null_expr(LogicalTypeId::Boolean),
        BinaryOperator::And,
        literal(LogicalTypeId::Boolean, false),
        type(LogicalTypeId::Boolean),
    };
    auto and_result = ExpressionEvaluator::evaluate_constant(null_and_false);
    require(and_result.has_value() && !get_value<bool>(*and_result), "NULL AND FALSE must be FALSE");

    auto null_or_false = BoundBinaryExpression {
        null_expr(LogicalTypeId::Boolean),
        BinaryOperator::Or,
        literal(LogicalTypeId::Boolean, false),
        type(LogicalTypeId::Boolean),
    };
    auto or_result = ExpressionEvaluator::evaluate_constant(null_or_false);
    require(or_result.has_value() && or_result->is_null(), "NULL OR FALSE must be NULL");

    ExpressionEvaluator evaluator;
    auto filter_null = evaluator.evaluate_filter(*null_expr(LogicalTypeId::Boolean));
    require(filter_null.has_value() && !*filter_null, "NULL filter must not match");
}

void test_in_between_like_and_cast()
{
    std::vector<std::unique_ptr<BoundExpression>> candidates;
    candidates.push_back(literal(LogicalTypeId::Integer, std::int32_t {1}));
    candidates.push_back(null_expr(LogicalTypeId::Integer));
    BoundInExpression in {
        literal(LogicalTypeId::Integer, std::int32_t {2}),
        std::move(candidates),
    };
    auto in_result = ExpressionEvaluator::evaluate_constant(in);
    require(in_result.has_value() && in_result->is_null(), "IN with no match and NULL must be NULL");

    BoundBetweenExpression between {
        literal(LogicalTypeId::Integer, std::int32_t {18}),
        literal(LogicalTypeId::Integer, std::int32_t {10}),
        literal(LogicalTypeId::Integer, std::int32_t {20}),
    };
    auto between_result = ExpressionEvaluator::evaluate_constant(between);
    require(between_result.has_value() && get_value<bool>(*between_result), "BETWEEN failed");

    BoundLikeExpression like {
        literal(LogicalTypeId::Varchar, std::string {"alice"}),
        literal(LogicalTypeId::Varchar, std::string {"a%_e"}),
    };
    auto like_result = ExpressionEvaluator::evaluate_constant(like);
    require(like_result.has_value() && get_value<bool>(*like_result), "LIKE failed");

    BoundCastExpression cast {
        literal(LogicalTypeId::Integer, std::int32_t {18}),
        type(LogicalTypeId::Double),
    };
    auto cast_result = ExpressionEvaluator::evaluate_constant(cast);
    require(cast_result.has_value() && get_value<double>(*cast_result) == 18.0, "widening cast failed");
}

void test_scalar_function_and_errors()
{
    const std::vector<LogicalType> argument_types {
        type(LogicalTypeId::Vector, 3),
        type(LogicalTypeId::Vector, 3),
    };
    auto binding = litedb::core::function::builtin::builtin_function_catalog().bind_scalar(
        "l2_distance",
        argument_types
    );
    require(binding.has_value(), "l2_distance binding missing");

    std::vector<std::unique_ptr<BoundExpression>> arguments;
    arguments.push_back(literal(LogicalTypeId::Vector, VectorValue {1.0, 2.0, 3.0}));
    arguments.push_back(literal(LogicalTypeId::Vector, VectorValue {1.0, 2.0, 5.0}));
    BoundFunctionExpression function {
        std::move(*binding),
        std::move(arguments),
    };
    auto result = ExpressionEvaluator::evaluate_constant(function);
    require(result.has_value() && get_value<double>(*result) == 2.0, "scalar function failed");

    auto values = row_values();
    ExpressionEvaluator evaluator {EvaluationContext {.input_values = values}};
    auto missing = evaluator.evaluate(*column(7, 99, LogicalTypeId::Integer));
    require(!missing.has_value(), "invalid ordinal should fail");
    require(missing.error().is(EvaluationErrorCode::InvalidColumnReference), "invalid ordinal error mismatch");
}

void test_unary_operators_and_boundaries()
{
    BoundUnaryExpression logical_not {
        UnaryOperator::Not,
        literal(LogicalTypeId::Boolean, true),
        type(LogicalTypeId::Boolean),
    };
    require_boolean(ExpressionEvaluator::evaluate_constant(logical_not), false, "NOT TRUE must be FALSE");

    BoundUnaryExpression null_not {
        UnaryOperator::Not,
        null_expr(LogicalTypeId::Boolean),
        type(LogicalTypeId::Boolean),
    };
    require_boolean(ExpressionEvaluator::evaluate_constant(null_not), std::nullopt, "NOT NULL must be NULL");

    BoundUnaryExpression integer_negate {
        UnaryOperator::Negate,
        literal(LogicalTypeId::Integer, std::int32_t {7}),
        type(LogicalTypeId::Integer),
    };
    auto integer_result = ExpressionEvaluator::evaluate_constant(integer_negate);
    require(integer_result.has_value() && get_value<std::int32_t>(*integer_result) == -7, "integer negate failed");

    BoundUnaryExpression integer_minimum {
        UnaryOperator::Negate,
        literal(LogicalTypeId::Integer, std::numeric_limits<std::int32_t>::min()),
        type(LogicalTypeId::Integer),
    };
    require_error(
        ExpressionEvaluator::evaluate_constant(integer_minimum),
        EvaluationErrorCode::NumericOverflow,
        "integer minimum negate must overflow"
    );

    BoundUnaryExpression bigint_minimum {
        UnaryOperator::Negate,
        literal(LogicalTypeId::BigInt, std::numeric_limits<std::int64_t>::min()),
        type(LogicalTypeId::BigInt),
    };
    require_error(
        ExpressionEvaluator::evaluate_constant(bigint_minimum),
        EvaluationErrorCode::NumericOverflow,
        "BIGINT minimum negate must overflow"
    );

    BoundUnaryExpression invalid_not {
        UnaryOperator::Not,
        literal(LogicalTypeId::Integer, std::int32_t {1}),
        type(LogicalTypeId::Boolean),
    };
    require_error(
        ExpressionEvaluator::evaluate_constant(invalid_not),
        EvaluationErrorCode::InvalidType,
        "NOT non-boolean must fail"
    );
}

void test_integer_arithmetic_matrix_and_boundaries()
{
    struct ArithmeticCase
    {
        BinaryOperator op;
        std::int32_t expected;
    };
    constexpr std::array cases {
        ArithmeticCase {BinaryOperator::Add, 12},
        ArithmeticCase {BinaryOperator::Subtract, 2},
        ArithmeticCase {BinaryOperator::Multiply, 35},
        ArithmeticCase {BinaryOperator::Divide, 1},
        ArithmeticCase {BinaryOperator::Modulus, 2},
    };
    for (const auto & test_case : cases) {
        auto expression = binary(
            literal(LogicalTypeId::Integer, std::int32_t {7}),
            test_case.op,
            literal(LogicalTypeId::Integer, std::int32_t {5}),
            LogicalTypeId::Integer
        );
        auto result = ExpressionEvaluator::evaluate_constant(expression);
        require(result.has_value(), "integer arithmetic failed");
        require(get_value<std::int32_t>(*result) == test_case.expected, "integer arithmetic result mismatch");
    }

    const auto maximum = std::numeric_limits<std::int32_t>::max();
    const auto minimum = std::numeric_limits<std::int32_t>::min();
    struct OverflowCase
    {
        BinaryOperator op;
        std::int32_t left;
        std::int32_t right;
    };
    constexpr std::array overflow_cases {
        OverflowCase {BinaryOperator::Add, maximum, 1},
        OverflowCase {BinaryOperator::Subtract, minimum, 1},
        OverflowCase {BinaryOperator::Multiply, maximum, 2},
        OverflowCase {BinaryOperator::Multiply, minimum, -1},
        OverflowCase {BinaryOperator::Divide, minimum, -1},
    };
    for (const auto & test_case : overflow_cases) {
        auto expression = binary(
            literal(LogicalTypeId::Integer, test_case.left),
            test_case.op,
            literal(LogicalTypeId::Integer, test_case.right),
            LogicalTypeId::Integer
        );
        require_error(
            ExpressionEvaluator::evaluate_constant(expression),
            EvaluationErrorCode::NumericOverflow,
            "integer boundary must overflow"
        );
    }

    auto modulus_minimum = binary(
        literal(LogicalTypeId::Integer, minimum),
        BinaryOperator::Modulus,
        literal(LogicalTypeId::Integer, std::int32_t {-1}),
        LogicalTypeId::Integer
    );
    auto modulus_result = ExpressionEvaluator::evaluate_constant(modulus_minimum);
    require(modulus_result.has_value() && get_value<std::int32_t>(*modulus_result) == 0, "MIN % -1 must be zero");

    for (const auto op : {BinaryOperator::Divide, BinaryOperator::Modulus}) {
        auto expression = binary(
            literal(LogicalTypeId::Integer, std::int32_t {1}),
            op,
            literal(LogicalTypeId::Integer, std::int32_t {0}),
            LogicalTypeId::Integer
        );
        require_error(
            ExpressionEvaluator::evaluate_constant(expression),
            EvaluationErrorCode::DivisionByZero,
            "integer division or modulus by zero must fail"
        );
    }

    const auto bigint_maximum = std::numeric_limits<std::int64_t>::max();
    const auto bigint_minimum = std::numeric_limits<std::int64_t>::min();
    struct BigIntOverflowCase
    {
        BinaryOperator op;
        std::int64_t left;
        std::int64_t right;
    };
    constexpr std::array bigint_overflow_cases {
        BigIntOverflowCase {BinaryOperator::Add, bigint_maximum, 1},
        BigIntOverflowCase {BinaryOperator::Subtract, bigint_minimum, 1},
        BigIntOverflowCase {BinaryOperator::Multiply, bigint_maximum, 2},
        BigIntOverflowCase {BinaryOperator::Divide, bigint_minimum, -1},
    };
    for (const auto & test_case : bigint_overflow_cases) {
        auto expression = binary(
            literal(LogicalTypeId::BigInt, test_case.left),
            test_case.op,
            literal(LogicalTypeId::BigInt, test_case.right),
            LogicalTypeId::BigInt
        );
        require_error(
            ExpressionEvaluator::evaluate_constant(expression),
            EvaluationErrorCode::NumericOverflow,
            "BIGINT boundary must overflow"
        );
    }
}

void test_floating_arithmetic_and_unsupported_operator()
{
    auto float_add = binary(
        literal(LogicalTypeId::Float, 1.25F),
        BinaryOperator::Add,
        literal(LogicalTypeId::Float, 2.5F),
        LogicalTypeId::Float
    );
    auto float_result = ExpressionEvaluator::evaluate_constant(float_add);
    require(float_result.has_value(), "FLOAT add failed");
    require(std::fabs(get_value<float>(*float_result) - 3.75F) < 0.0001F, "FLOAT add mismatch");

    auto double_modulus = binary(
        literal(LogicalTypeId::Double, 7.5),
        BinaryOperator::Modulus,
        literal(LogicalTypeId::Double, 2.0),
        LogicalTypeId::Double
    );
    auto double_result = ExpressionEvaluator::evaluate_constant(double_modulus);
    require(double_result.has_value(), "DOUBLE modulus failed");
    require(std::fabs(get_value<double>(*double_result) - 1.5) < 0.000001, "DOUBLE modulus mismatch");

    auto divide_zero = binary(
        literal(LogicalTypeId::Double, 1.0),
        BinaryOperator::Divide,
        literal(LogicalTypeId::Double, 0.0),
        LogicalTypeId::Double
    );
    require_error(
        ExpressionEvaluator::evaluate_constant(divide_zero),
        EvaluationErrorCode::DivisionByZero,
        "floating division by zero must fail"
    );

    auto power = binary(
        literal(LogicalTypeId::Integer, std::int32_t {2}),
        BinaryOperator::Power,
        literal(LogicalTypeId::Integer, std::int32_t {3}),
        LogicalTypeId::Integer
    );
    require_error(
        ExpressionEvaluator::evaluate_constant(power),
        EvaluationErrorCode::UnsupportedExpression,
        "unsupported POWER must fail explicitly"
    );
}

void test_comparison_matrix()
{
    struct ComparisonCase
    {
        BinaryOperator op;
        bool expected;
    };
    constexpr std::array cases {
        ComparisonCase {BinaryOperator::Equal, false},
        ComparisonCase {BinaryOperator::NotEqual, true},
        ComparisonCase {BinaryOperator::LessThan, true},
        ComparisonCase {BinaryOperator::LessThanOrEqual, true},
        ComparisonCase {BinaryOperator::GreaterThan, false},
        ComparisonCase {BinaryOperator::GreaterThanOrEqual, false},
    };
    for (const auto & test_case : cases) {
        auto expression = binary(
            literal(LogicalTypeId::BigInt, std::int64_t {4}),
            test_case.op,
            literal(LogicalTypeId::BigInt, std::int64_t {5}),
            LogicalTypeId::Boolean
        );
        require_boolean(
            ExpressionEvaluator::evaluate_constant(expression),
            test_case.expected,
            "BIGINT comparison mismatch"
        );
    }

    auto string_order = binary(
        literal(LogicalTypeId::Varchar, std::string {"alice"}),
        BinaryOperator::LessThan,
        literal(LogicalTypeId::Varchar, std::string {"bob"}),
        LogicalTypeId::Boolean
    );
    require_boolean(ExpressionEvaluator::evaluate_constant(string_order), true, "VARCHAR ordering failed");

    auto boolean_equal = binary(
        literal(LogicalTypeId::Boolean, true),
        BinaryOperator::Equal,
        literal(LogicalTypeId::Boolean, true),
        LogicalTypeId::Boolean
    );
    require_boolean(ExpressionEvaluator::evaluate_constant(boolean_equal), true, "BOOLEAN equality failed");

    auto vector_equal = binary(
        literal(LogicalTypeId::Vector, VectorValue {1.0, 2.0}),
        BinaryOperator::Equal,
        literal(LogicalTypeId::Vector, VectorValue {1.0, 2.0}),
        LogicalTypeId::Boolean
    );
    require_boolean(ExpressionEvaluator::evaluate_constant(vector_equal), true, "VECTOR equality failed");

    auto null_compare = binary(
        null_expr(LogicalTypeId::Integer),
        BinaryOperator::Equal,
        literal(LogicalTypeId::Integer, std::int32_t {1}),
        LogicalTypeId::Boolean
    );
    require_boolean(ExpressionEvaluator::evaluate_constant(null_compare), std::nullopt, "NULL comparison must be NULL");

    auto mismatched = binary(
        literal(LogicalTypeId::Integer, std::int32_t {1}),
        BinaryOperator::Equal,
        literal(LogicalTypeId::BigInt, std::int64_t {1}),
        LogicalTypeId::Boolean
    );
    require_error(
        ExpressionEvaluator::evaluate_constant(mismatched),
        EvaluationErrorCode::InvalidType,
        "runtime comparison type mismatch must fail"
    );
}

void test_complete_three_value_logic_and_short_circuit()
{
    constexpr std::array<std::optional<bool>, 3> values {
        std::nullopt,
        false,
        true,
    };
    constexpr std::array<std::array<std::optional<bool>, 3>, 3> and_results {{
        {std::nullopt, false, std::nullopt},
        {false, false, false},
        {std::nullopt, false, true},
    }};
    constexpr std::array<std::array<std::optional<bool>, 3>, 3> or_results {{
        {std::nullopt, std::nullopt, true},
        {std::nullopt, false, true},
        {true, true, true},
    }};

    for (std::size_t left = 0; left < values.size(); ++left) {
        for (std::size_t right = 0; right < values.size(); ++right) {
            auto and_expression = binary(
                boolean_expression(values[left]),
                BinaryOperator::And,
                boolean_expression(values[right]),
                LogicalTypeId::Boolean
            );
            require_boolean(
                ExpressionEvaluator::evaluate_constant(and_expression),
                and_results[left][right],
                "AND truth table mismatch"
            );

            auto or_expression = binary(
                boolean_expression(values[left]),
                BinaryOperator::Or,
                boolean_expression(values[right]),
                LogicalTypeId::Boolean
            );
            require_boolean(
                ExpressionEvaluator::evaluate_constant(or_expression),
                or_results[left][right],
                "OR truth table mismatch"
            );
        }
    }

    auto failing_right = [] {
        return std::make_unique<BoundBinaryExpression>(
            literal(LogicalTypeId::Integer, std::int32_t {1}),
            BinaryOperator::Divide,
            literal(LogicalTypeId::Integer, std::int32_t {0}),
            type(LogicalTypeId::Integer)
        );
    };
    auto short_and = binary(
        literal(LogicalTypeId::Boolean, false),
        BinaryOperator::And,
        failing_right(),
        LogicalTypeId::Boolean
    );
    require_boolean(ExpressionEvaluator::evaluate_constant(short_and), false, "FALSE AND must short-circuit");

    auto short_or = binary(
        literal(LogicalTypeId::Boolean, true),
        BinaryOperator::Or,
        failing_right(),
        LogicalTypeId::Boolean
    );
    require_boolean(ExpressionEvaluator::evaluate_constant(short_or), true, "TRUE OR must short-circuit");

    ExpressionEvaluator evaluator;
    auto true_filter = evaluator.evaluate_filter(*literal(LogicalTypeId::Boolean, true));
    require(true_filter.has_value() && *true_filter, "TRUE filter must match");
    auto false_filter = evaluator.evaluate_filter(*literal(LogicalTypeId::Boolean, false));
    require(false_filter.has_value() && !*false_filter, "FALSE filter must not match");
    auto invalid_filter = evaluator.evaluate_filter(*literal(LogicalTypeId::Integer, std::int32_t {1}));
    require(!invalid_filter.has_value(), "non-boolean filter must fail");
    require(invalid_filter.error().is(EvaluationErrorCode::InvalidType), "filter error mismatch");
}

void test_in_and_between_null_semantics()
{
    auto make_in = [](std::unique_ptr<BoundExpression> target, std::vector<std::unique_ptr<BoundExpression>> values) {
        return BoundInExpression {std::move(target), std::move(values)};
    };

    std::vector<std::unique_ptr<BoundExpression>> matching_values;
    matching_values.push_back(literal(LogicalTypeId::Integer, std::int32_t {1}));
    matching_values.push_back(literal(LogicalTypeId::Integer, std::int32_t {2}));
    auto matching = make_in(literal(LogicalTypeId::Integer, std::int32_t {2}), std::move(matching_values));
    require_boolean(ExpressionEvaluator::evaluate_constant(matching), true, "matching IN must be TRUE");

    std::vector<std::unique_ptr<BoundExpression>> missing_values;
    missing_values.push_back(literal(LogicalTypeId::Integer, std::int32_t {1}));
    auto missing = make_in(literal(LogicalTypeId::Integer, std::int32_t {2}), std::move(missing_values));
    require_boolean(ExpressionEvaluator::evaluate_constant(missing), false, "missing IN must be FALSE");

    std::vector<std::unique_ptr<BoundExpression>> empty_values;
    auto empty = make_in(literal(LogicalTypeId::Integer, std::int32_t {2}), std::move(empty_values));
    require_boolean(ExpressionEvaluator::evaluate_constant(empty), false, "empty IN must be FALSE");

    std::vector<std::unique_ptr<BoundExpression>> null_target_values;
    null_target_values.push_back(literal(LogicalTypeId::Integer, std::int32_t {1}));
    auto null_target = make_in(null_expr(LogicalTypeId::Integer), std::move(null_target_values));
    require_boolean(ExpressionEvaluator::evaluate_constant(null_target), std::nullopt, "NULL IN must be NULL");

    BoundBetweenExpression lower_null {
        literal(LogicalTypeId::Integer, std::int32_t {18}),
        null_expr(LogicalTypeId::Integer),
        literal(LogicalTypeId::Integer, std::int32_t {20}),
    };
    require_boolean(ExpressionEvaluator::evaluate_constant(lower_null), std::nullopt, "BETWEEN NULL lower must be NULL");

    BoundBetweenExpression false_with_null_upper {
        literal(LogicalTypeId::Integer, std::int32_t {5}),
        literal(LogicalTypeId::Integer, std::int32_t {10}),
        null_expr(LogicalTypeId::Integer),
    };
    require_boolean(
        ExpressionEvaluator::evaluate_constant(false_with_null_upper),
        false,
        "FALSE AND NULL BETWEEN result must be FALSE"
    );
}

void test_like_pattern_matrix()
{
    struct LikeCase
    {
        const char * value;
        const char * pattern;
        bool expected;
    };
    constexpr std::array cases {
        LikeCase {"", "", true},
        LikeCase {"", "%", true},
        LikeCase {"a", "", false},
        LikeCase {"alice", "alice", true},
        LikeCase {"alice", "a%", true},
        LikeCase {"alice", "%ice", true},
        LikeCase {"alice", "a__ce", true},
        LikeCase {"alice", "a%%e", true},
        LikeCase {"alice", "b%", false},
        LikeCase {"alice", "a_d", false},
    };
    for (const auto & test_case : cases) {
        BoundLikeExpression expression {
            literal(LogicalTypeId::Varchar, std::string {test_case.value}),
            literal(LogicalTypeId::Varchar, std::string {test_case.pattern}),
        };
        require_boolean(
            ExpressionEvaluator::evaluate_constant(expression),
            test_case.expected,
            "LIKE pattern result mismatch"
        );
    }

    BoundLikeExpression null_value {
        null_expr(LogicalTypeId::Varchar),
        literal(LogicalTypeId::Varchar, std::string {"%"}),
    };
    require_boolean(ExpressionEvaluator::evaluate_constant(null_value), std::nullopt, "NULL LIKE must be NULL");

    BoundLikeExpression invalid_type {
        literal(LogicalTypeId::Integer, std::int32_t {1}),
        literal(LogicalTypeId::Varchar, std::string {"%"}),
    };
    require_error(
        ExpressionEvaluator::evaluate_constant(invalid_type),
        EvaluationErrorCode::InvalidType,
        "LIKE non-string must fail"
    );
}

void test_cast_matrix_and_vector_validation()
{
    auto int_to_bigint = BoundCastExpression {
        literal(LogicalTypeId::Integer, std::int32_t {18}),
        type(LogicalTypeId::BigInt),
    };
    auto bigint_result = ExpressionEvaluator::evaluate_constant(int_to_bigint);
    require(bigint_result.has_value() && get_value<std::int64_t>(*bigint_result) == 18, "INTEGER to BIGINT failed");

    auto bigint_to_double = BoundCastExpression {
        literal(LogicalTypeId::BigInt, std::int64_t {18}),
        type(LogicalTypeId::Double),
    };
    auto double_result = ExpressionEvaluator::evaluate_constant(bigint_to_double);
    require(double_result.has_value() && get_value<double>(*double_result) == 18.0, "BIGINT to DOUBLE failed");

    auto float_to_double = BoundCastExpression {
        literal(LogicalTypeId::Float, 1.5F),
        type(LogicalTypeId::Double),
    };
    auto widened_float = ExpressionEvaluator::evaluate_constant(float_to_double);
    require(widened_float.has_value() && get_value<double>(*widened_float) == 1.5, "FLOAT to DOUBLE failed");

    auto null_cast = BoundCastExpression {
        null_expr(LogicalTypeId::Integer),
        type(LogicalTypeId::Double),
    };
    auto null_result = ExpressionEvaluator::evaluate_constant(null_cast);
    require(null_result.has_value() && null_result->is_null(), "NULL cast must remain NULL");

    auto narrowing = BoundCastExpression {
        literal(LogicalTypeId::Double, 1.5),
        type(LogicalTypeId::Integer),
    };
    require_error(
        ExpressionEvaluator::evaluate_constant(narrowing),
        EvaluationErrorCode::CastFailed,
        "unsupported narrowing cast must fail"
    );

    auto vector_dimension = BoundCastExpression {
        literal(LogicalTypeId::Vector, VectorValue {1.0, 2.0}),
        type(LogicalTypeId::Vector, 3),
    };
    require_error(
        ExpressionEvaluator::evaluate_constant(vector_dimension),
        EvaluationErrorCode::CastFailed,
        "vector dimension mismatch must fail"
    );

    std::vector<std::unique_ptr<BoundExpression>> null_elements;
    null_elements.push_back(literal(LogicalTypeId::Double, 1.0));
    null_elements.push_back(null_expr(LogicalTypeId::Double));
    BoundVectorExpression null_vector {std::move(null_elements)};
    auto null_vector_result = ExpressionEvaluator::evaluate_constant(null_vector);
    require(null_vector_result.has_value() && null_vector_result->is_null(), "vector with NULL element must be NULL");

    std::vector<std::unique_ptr<BoundExpression>> invalid_elements;
    invalid_elements.push_back(literal(LogicalTypeId::Integer, std::int32_t {1}));
    BoundVectorExpression invalid_vector {std::move(invalid_elements)};
    require_error(
        ExpressionEvaluator::evaluate_constant(invalid_vector),
        EvaluationErrorCode::InvalidType,
        "vector non-DOUBLE element must fail"
    );
}

std::expected<Value, litedb::core::function::FunctionError> failing_function(
    std::span<const Value>,
    const litedb::core::function::ScalarFunctionContext &,
    const litedb::core::function::FunctionBindData *
)
{
    return std::unexpected(litedb::core::function::FunctionError {
        litedb::core::function::FunctionErrorCode::InvalidArgument,
        "intentional function failure",
    });
}

std::expected<Value, litedb::core::function::FunctionError> wrong_type_function(
    std::span<const Value>,
    const litedb::core::function::ScalarFunctionContext &,
    const litedb::core::function::FunctionBindData *
)
{
    return Value {ValueData {std::int32_t {1}}};
}

std::expected<Value, litedb::core::function::FunctionError> invalid_type_function(
    std::span<const Value>,
    const litedb::core::function::ScalarFunctionContext &,
    const litedb::core::function::FunctionBindData *
)
{
    return std::unexpected(litedb::core::function::FunctionError {
        litedb::core::function::FunctionErrorCode::InvalidType,
        "intentional function type failure",
    });
}

litedb::core::function::BoundScalarFunction make_test_binding(
    std::string name,
    LogicalType return_type,
    litedb::core::function::ScalarFunctionOverload::EvalFn evaluate
)
{
    litedb::core::function::FunctionCatalogBuilder builder;
    auto registered = builder.register_scalar(
        name,
        litedb::core::function::ScalarFunctionOverload {
            .parameters = {},
            .return_type = return_type,
            .bind = nullptr,
            .evaluate = evaluate,
            .properties = {},
        }
    );
    require(registered.has_value(), "test function registration failed");
    auto catalog = std::move(builder).build();
    require(catalog.has_value(), "test function catalog build failed");
    auto binding = catalog->bind_scalar(name, {});
    require(binding.has_value(), "test function binding failed");
    return std::move(*binding);
}

void test_function_error_propagation_and_runtime_validation()
{
    auto failing = make_test_binding(
        "failing",
        type(LogicalTypeId::Integer),
        failing_function
    );
    BoundFunctionExpression failing_expression {
        std::move(failing),
        {},
    };
    auto failure = ExpressionEvaluator::evaluate_constant(failing_expression);
    require(!failure.has_value(), "function error must be propagated");
    require(
        failure.error().category() == litedb::core::error::ErrorCategory::Function,
        "function error category must be preserved"
    );
    require(
        failure.error().is(litedb::core::function::FunctionErrorCode::InvalidArgument),
        "function error code must be preserved"
    );
    require(failure.error().message() == "intentional function failure", "function error message must be preserved");
    require(failure.error().cause() == nullptr, "propagated function error must not be wrapped");

    auto invalid_type = make_test_binding(
        "invalid_type",
        type(LogicalTypeId::Integer),
        invalid_type_function
    );
    BoundFunctionExpression invalid_type_expression {
        std::move(invalid_type),
        {},
    };
    auto invalid_type_result = ExpressionEvaluator::evaluate_constant(invalid_type_expression);
    require(!invalid_type_result.has_value(), "function InvalidType must be propagated");
    require(
        invalid_type_result.error().category() == litedb::core::error::ErrorCategory::Function,
        "function InvalidType category must be preserved"
    );
    require(
        invalid_type_result.error().is(litedb::core::function::FunctionErrorCode::InvalidType),
        "function InvalidType code must be preserved"
    );
    require(
        invalid_type_result.error().message() == "intentional function type failure",
        "function InvalidType message must be preserved"
    );
    require(invalid_type_result.error().cause() == nullptr, "propagated function InvalidType must not be wrapped");

    auto wrong_type = make_test_binding(
        "wrong_type",
        type(LogicalTypeId::Double),
        wrong_type_function
    );
    BoundFunctionExpression wrong_type_expression {
        std::move(wrong_type),
        {},
    };
    require_error(
        ExpressionEvaluator::evaluate_constant(wrong_type_expression),
        EvaluationErrorCode::InvalidType,
        "function return type mismatch must fail"
    );

    BoundLiteralExpression invalid_literal {
        type(LogicalTypeId::Integer),
        Value {ValueData {std::string {"not an integer"}}},
    };
    require_error(
        ExpressionEvaluator::evaluate_constant(invalid_literal),
        EvaluationErrorCode::InvalidType,
        "bound literal runtime type mismatch must fail"
    );

    auto values = row_values();
    ExpressionEvaluator evaluator {EvaluationContext {.input_values = values}};
    auto wrong_column_type = evaluator.evaluate(*column(1, 0, LogicalTypeId::Varchar));
    require_error(
        wrong_column_type,
        EvaluationErrorCode::InvalidType,
        "column runtime type mismatch must fail"
    );
}

} // namespace

int main()
{
    try {
        test_literals_vector_and_ordinal_column_ref();
        test_exact_numeric_semantics_and_overflow();
        test_three_value_logic_and_filter_semantics();
        test_in_between_like_and_cast();
        test_scalar_function_and_errors();
        test_unary_operators_and_boundaries();
        test_integer_arithmetic_matrix_and_boundaries();
        test_floating_arithmetic_and_unsupported_operator();
        test_comparison_matrix();
        test_complete_three_value_logic_and_short_circuit();
        test_in_and_between_null_semantics();
        test_like_pattern_matrix();
        test_cast_matrix_and_vector_validation();
        test_function_error_propagation_and_runtime_validation();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
    return 0;
}
