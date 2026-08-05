#include "core/function/builtin/builtin_functions.hpp"

#include <iostream>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace
{

using namespace litedb::core;
using common::LogicalType;
using common::LogicalTypeId;
using common::Value;
using common::ValueData;
using common::VectorValue;

LogicalType type(LogicalTypeId id, std::optional<std::size_t> parameter = std::nullopt)
{
    return LogicalType {id, parameter};
}

void require(bool condition, const char * message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void test_vector_builtins()
{
    const auto & catalog = function::builtin::builtin_function_catalog();
    const std::vector<LogicalType> types {
        type(LogicalTypeId::Vector, 3),
        type(LogicalTypeId::Vector, 3),
    };
    auto binding = catalog.bind_scalar("L2_DISTANCE", types);
    require(binding.has_value(), "L2 distance binding failed");
    require(
        binding->properties().semantic_tag == function::FunctionSemanticTag::VectorL2Distance,
        "L2 semantic tag mismatch"
    );

    const std::vector<Value> arguments {
        Value {ValueData {VectorValue {1.0, 2.0, 3.0}}},
        Value {ValueData {VectorValue {1.0, 2.0, 5.0}}},
    };
    auto result = binding->evaluate(arguments, {});
    require(result.has_value(), "L2 evaluation failed");
    require(std::get<double>(result->data()) == 2.0, "L2 result mismatch");

    auto cosine = catalog.bind_scalar("cosine_distance", types);
    require(cosine.has_value(), "cosine distance binding failed");
    const std::vector<Value> orthogonal_arguments {
        Value {ValueData {VectorValue {1.0, 0.0, 0.0}}},
        Value {ValueData {VectorValue {0.0, 1.0, 0.0}}},
    };
    auto cosine_result = cosine->evaluate(orthogonal_arguments, {});
    require(cosine_result.has_value(), "cosine distance evaluation failed");
    require(std::get<double>(cosine_result->data()) == 1.0, "cosine distance result mismatch");

    auto inner = catalog.bind_scalar("inner_product", types);
    require(inner.has_value(), "inner product binding failed");
    auto inner_result = inner->evaluate(
        std::vector<Value> {
            Value {ValueData {VectorValue {1.0, 2.0, 3.0}}},
            Value {ValueData {VectorValue {4.0, 5.0, 6.0}}},
        },
        {}
    );
    require(inner_result.has_value(), "inner product evaluation failed");
    require(std::get<double>(inner_result->data()) == 32.0, "inner product result mismatch");

    const std::vector<Value> null_arguments {Value::null(), arguments[1]};
    auto null_result = binding->evaluate(null_arguments, {});
    require(null_result.has_value() && null_result->is_null(), "NULL propagation failed");

    const std::vector<LogicalType> invalid_types {
        type(LogicalTypeId::Vector, 3),
        type(LogicalTypeId::Vector, 4),
    };
    auto invalid = catalog.bind_scalar("l2_distance", invalid_types);
    require(!invalid.has_value(), "mismatched vector dimensions should fail binding");
    require(
        invalid.error().is(function::FunctionErrorCode::ConstraintViolation),
        "vector dimension error mismatch"
    );

    const std::vector<LogicalType> unknown_types {
        type(LogicalTypeId::Vector),
        type(LogicalTypeId::Vector),
    };
    auto unknown = catalog.bind_scalar("inner_product", unknown_types);
    require(unknown.has_value(), "unknown vector dimensions should bind");
    const std::vector<Value> mismatched_values {
        Value {ValueData {VectorValue {1.0, 2.0}}},
        Value {ValueData {VectorValue {1.0, 2.0, 3.0}}},
    };
    auto runtime_error = unknown->evaluate(mismatched_values, {});
    require(!runtime_error.has_value(), "runtime vector dimensions must be checked");
    require(
        runtime_error.error().is(function::FunctionErrorCode::InvalidArgument),
        "runtime vector dimension error mismatch"
    );
}

} // namespace

int main()
{
    try {
        test_vector_builtins();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
    return 0;
}
