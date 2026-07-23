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
#include "core/binder/bound/expression/bound_wildcard_expression.hpp"
#include "core/evaluator/expression_evaluator.hpp"
#include "core/function/builtin/builtin_functions.hpp"
#include "core/parser/token.hpp"

#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

using namespace litedb::core::binder::bound;
using namespace litedb::core::common;
using namespace litedb::core::evaluator;
using namespace litedb::core::parser;
using namespace litedb::core::parser::ast;

constexpr AstNodeLocation loc {1, 1};

void require(bool condition, const char * message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

LogicalType type(LogicalTypeId id, std::optional<std::size_t> parameter = std::nullopt)
{
    return LogicalType {id, parameter};
}

std::unique_ptr<BoundExpression> literal(LogicalTypeId type_id, std::string value)
{
    return std::make_unique<BoundLiteralExpression>(type(type_id), std::move(value), loc);
}

std::unique_ptr<BoundExpression> null_expr(LogicalTypeId type_id = LogicalTypeId::Null)
{
    return std::make_unique<BoundNullExpression>(type(type_id), loc);
}

std::unique_ptr<BoundExpression> column(
    ColumnId column_id,
    LogicalTypeId type_id,
    std::string name = "column"
)
{
    return std::make_unique<BoundColumnRefExpression>(
        1,
        1,
        "users",
        column_id,
        std::move(name),
        type(type_id),
        true,
        loc
    );
}

Record record()
{
    return Record {
        .record_id = 1,
        .data = RecordData {
            .values = {
                Value {std::int64_t {42}},
                Value {std::string {"alice"}},
                Value {std::int32_t {18}},
                Value {true},
                Value::null(),
            },
        },
    };
}

template <typename T>
const T & get_value(const Value & value)
{
    return std::get<T>(value.data());
}

void test_literals_null_vector_and_column_ref()
{
    ExpressionEvaluator evaluator;
    const auto row = record();

    auto integer = evaluator.evaluate(*literal(LogicalTypeId::Integer, "123"), row);
    require(integer.has_value(), "integer literal failed");
    require(get_value<std::int32_t>(*integer) == 123, "integer literal mismatch");

    auto varchar = evaluator.evaluate(*literal(LogicalTypeId::Varchar, "alice"), row);
    require(varchar.has_value(), "varchar literal failed");
    require(get_value<std::string>(*varchar) == "alice", "varchar literal mismatch");

    auto null_value = evaluator.evaluate(*null_expr(), row);
    require(null_value.has_value(), "null expression failed");
    require(null_value->is_null(), "null expression mismatch");

    std::vector<std::unique_ptr<BoundExpression>> elements;
    elements.push_back(literal(LogicalTypeId::Double, "0.1"));
    elements.push_back(literal(LogicalTypeId::Double, "0.2"));
    auto vector_expression = BoundVectorExpression {std::move(elements), type(LogicalTypeId::Vector, 2), loc};
    auto vector_value = evaluator.evaluate(vector_expression, row);
    require(vector_value.has_value(), "vector expression failed");
    require(get_value<VectorValue>(*vector_value).size() == 2, "vector size mismatch");

    auto column_value = evaluator.evaluate(*column(2, LogicalTypeId::Varchar, "name"), row);
    require(column_value.has_value(), "column ref failed");
    require(get_value<std::string>(*column_value) == "alice", "column ref mismatch");
}

void test_arithmetic_comparison_and_logic()
{
    ExpressionEvaluator evaluator;
    const auto row = record();

    auto add = BoundBinaryExpression {
        literal(LogicalTypeId::Integer, "2"),
        TokenType::Plus,
        literal(LogicalTypeId::Integer, "3"),
        type(LogicalTypeId::Integer),
        loc,
    };
    auto add_result = evaluator.evaluate(add, row);
    require(add_result.has_value(), "add failed");
    require(get_value<std::int32_t>(*add_result) == 5, "add mismatch");

    auto modulo = BoundBinaryExpression {
        literal(LogicalTypeId::BigInt, "7"),
        TokenType::Modulo,
        literal(LogicalTypeId::BigInt, "4"),
        type(LogicalTypeId::BigInt),
        loc,
    };
    auto modulo_result = evaluator.evaluate(modulo, row);
    require(modulo_result.has_value(), "modulo failed");
    require(get_value<std::int64_t>(*modulo_result) == 3, "modulo mismatch");

    auto greater = BoundBinaryExpression {
        literal(LogicalTypeId::Integer, "10"),
        TokenType::GreaterEqual,
        literal(LogicalTypeId::Integer, "10"),
        type(LogicalTypeId::Boolean),
        loc,
    };
    auto greater_result = evaluator.evaluate(greater, row);
    require(greater_result.has_value(), "comparison failed");
    require(get_value<bool>(*greater_result), "comparison mismatch");

    auto logical = BoundBinaryExpression {
        literal(LogicalTypeId::Boolean, "true"),
        TokenType::And,
        literal(LogicalTypeId::Boolean, "false"),
        type(LogicalTypeId::Boolean),
        loc,
    };
    auto logical_result = evaluator.evaluate(logical, row);
    require(logical_result.has_value(), "logical failed");
    require(!get_value<bool>(*logical_result), "logical mismatch");

    auto not_expression = BoundUnaryExpression {
        TokenType::Not,
        literal(LogicalTypeId::Boolean, "false"),
        type(LogicalTypeId::Boolean),
        loc,
    };
    auto not_result = evaluator.evaluate(not_expression, row);
    require(not_result.has_value(), "not failed");
    require(get_value<bool>(*not_result), "not mismatch");
}

void test_in_between_like_and_cast()
{
    ExpressionEvaluator evaluator;
    const auto row = record();

    std::vector<std::unique_ptr<BoundExpression>> values;
    values.push_back(literal(LogicalTypeId::Integer, "1"));
    values.push_back(literal(LogicalTypeId::Integer, "18"));
    auto in_expression = BoundInExpression {column(3, LogicalTypeId::Integer, "age"), std::move(values), loc};
    auto in_result = evaluator.evaluate(in_expression, row);
    require(in_result.has_value(), "in failed");
    require(get_value<bool>(*in_result), "in mismatch");

    auto between = BoundBetweenExpression {
        column(3, LogicalTypeId::Integer, "age"),
        literal(LogicalTypeId::Integer, "10"),
        literal(LogicalTypeId::Integer, "20"),
        loc,
    };
    auto between_result = evaluator.evaluate(between, row);
    require(between_result.has_value(), "between failed");
    require(get_value<bool>(*between_result), "between mismatch");

    auto like = BoundLikeExpression {
        column(2, LogicalTypeId::Varchar, "name"),
        literal(LogicalTypeId::Varchar, "a%_e"),
        loc,
    };
    auto like_result = evaluator.evaluate(like, row);
    require(like_result.has_value(), "like failed");
    require(get_value<bool>(*like_result), "like mismatch");

    auto cast = BoundCastExpression {
        literal(LogicalTypeId::Integer, "18"),
        type(LogicalTypeId::Double),
        loc,
    };
    auto cast_result = evaluator.evaluate(cast, row);
    require(cast_result.has_value(), "cast failed");
    require(get_value<double>(*cast_result) == 18.0, "cast mismatch");
}

void test_scalar_function()
{
    ExpressionEvaluator evaluator;
    const auto row = Record {
        .record_id = 1,
        .data = RecordData {
            .values = {
                Value {VectorValue {1.0, 2.0, 3.0}},
            },
        },
    };

    auto registry = litedb::core::function::builtin::make_builtin_function_registry();
    auto binding = registry.bind_scalar("l2_distance", {type(LogicalTypeId::Vector, 3), type(LogicalTypeId::Vector, 3)});
    require(binding.has_value(), "l2_distance binding missing");

    std::vector<std::unique_ptr<BoundExpression>> arguments;
    arguments.push_back(column(1, LogicalTypeId::Vector, "embedding"));
    std::vector<std::unique_ptr<BoundExpression>> elements;
    elements.push_back(literal(LogicalTypeId::Double, "1.0"));
    elements.push_back(literal(LogicalTypeId::Double, "2.0"));
    elements.push_back(literal(LogicalTypeId::Double, "5.0"));
    arguments.push_back(std::make_unique<BoundVectorExpression>(std::move(elements), type(LogicalTypeId::Vector, 3), loc));

    auto function = BoundFunctionExpression {
        "l2_distance",
        binding->function,
        binding->signature,
        std::move(arguments),
        type(LogicalTypeId::Double),
        loc,
    };
    auto result = evaluator.evaluate(function, row);
    require(result.has_value(), "l2_distance evaluation failed");
    require(get_value<double>(*result) == 2.0, "l2_distance value mismatch");
}

void test_predicate_and_null_semantics()
{
    ExpressionEvaluator evaluator;
    const auto row = record();

    auto true_result = evaluator.evaluate_predicate(*literal(LogicalTypeId::Boolean, "true"), row);
    require(true_result.has_value(), "true predicate failed");
    require(*true_result, "true predicate mismatch");

    auto false_result = evaluator.evaluate_predicate(*literal(LogicalTypeId::Boolean, "false"), row);
    require(false_result.has_value(), "false predicate failed");
    require(!*false_result, "false predicate mismatch");

    auto null_result = evaluator.evaluate_predicate(*null_expr(LogicalTypeId::Boolean), row);
    require(null_result.has_value(), "null predicate failed");
    require(!*null_result, "null predicate should be false");

    auto null_comparison = BoundBinaryExpression {
        null_expr(LogicalTypeId::Integer),
        TokenType::Equal,
        literal(LogicalTypeId::Integer, "1"),
        type(LogicalTypeId::Boolean),
        loc,
    };
    auto comparison_result = evaluator.evaluate(null_comparison, row);
    require(comparison_result.has_value(), "null comparison failed");
    require(comparison_result->is_null(), "null comparison should be null");
}

void test_errors()
{
    ExpressionEvaluator evaluator;
    const auto row = record();

    auto invalid_literal = evaluator.evaluate(*literal(LogicalTypeId::Integer, "not-an-int"), row);
    require(!invalid_literal.has_value(), "invalid literal should fail");
    require(invalid_literal.error().code == EvaluationErrorCode::InvalidLiteral, "invalid literal error mismatch");

    auto divide_by_zero = BoundBinaryExpression {
        literal(LogicalTypeId::Integer, "1"),
        TokenType::Slash,
        literal(LogicalTypeId::Integer, "0"),
        type(LogicalTypeId::Integer),
        loc,
    };
    auto divide_result = evaluator.evaluate(divide_by_zero, row);
    require(!divide_result.has_value(), "divide by zero should fail");
    require(divide_result.error().code == EvaluationErrorCode::DivisionByZero, "divide by zero error mismatch");

    auto bad_column = evaluator.evaluate(*column(99, LogicalTypeId::Integer, "missing"), row);
    require(!bad_column.has_value(), "bad column should fail");
    require(bad_column.error().code == EvaluationErrorCode::InvalidColumnReference, "bad column error mismatch");

    auto wildcard = BoundWildcardExpression {std::nullopt, loc};
    auto wildcard_result = evaluator.evaluate(wildcard, row);
    require(!wildcard_result.has_value(), "wildcard should fail");
    require(wildcard_result.error().code == EvaluationErrorCode::UnsupportedExpression, "wildcard error mismatch");
}

} // namespace

int main()
{
    try {
        test_literals_null_vector_and_column_ref();
        test_arithmetic_comparison_and_logic();
        test_in_between_like_and_cast();
        test_scalar_function();
        test_predicate_and_null_semantics();
        test_errors();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
