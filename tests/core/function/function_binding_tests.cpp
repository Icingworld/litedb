#include "core/function/function_catalog.hpp"

#include <expected>
#include <iostream>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace
{

using namespace litedb::core;
using common::LogicalType;
using common::LogicalTypeId;
using common::Value;

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

std::expected<Value, function::FunctionError> evaluate_null(
    std::span<const Value>,
    const function::ScalarFunctionContext &,
    const function::FunctionBindData *
)
{
    return Value::null();
}

std::expected<function::ScalarBindResult, function::FunctionError> bind_same_vector_dimension(
    std::span<const LogicalType> arguments
)
{
    if (arguments.size() != 2
        || arguments[0].id != LogicalTypeId::Vector
        || arguments[1].id != LogicalTypeId::Vector) {
        return std::unexpected(function::FunctionError {
            function::FunctionErrorCode::ConstraintViolation,
            "expected two vectors",
        });
    }
    if (arguments[0].parameter.has_value()
        && arguments[1].parameter.has_value()
        && arguments[0].parameter != arguments[1].parameter) {
        return std::unexpected(function::FunctionError {
            function::FunctionErrorCode::ConstraintViolation,
            "vector dimensions differ",
        });
    }
    return function::ScalarBindResult {
        .argument_types = {arguments[0], arguments[1]},
        .return_type = type(LogicalTypeId::Double),
        .bind_data = nullptr,
    };
}

void test_custom_bind_constraints()
{
    function::FunctionCatalogBuilder builder;
    require(builder.register_scalar(
        "distance",
        function::ScalarFunctionOverload {
            .parameters = function::FunctionParameters {
                .fixed = {type(LogicalTypeId::Vector), type(LogicalTypeId::Vector)},
                .variadic = std::nullopt,
            },
            .return_type = type(LogicalTypeId::Double),
            .bind = bind_same_vector_dimension,
            .evaluate = evaluate_null,
            .properties = function::FunctionProperties {
                .volatility = function::FunctionVolatility::Immutable,
                .null_handling = function::FunctionNullHandling::PropagateNull,
                .has_side_effects = false,
                .semantic_tag = function::FunctionSemanticTag::VectorL2Distance,
            },
        }
    ).has_value(), "custom overload registration failed");
    auto catalog = std::move(builder).build();
    require(catalog.has_value(), "catalog build failed");

    const std::vector<LogicalType> valid {
        type(LogicalTypeId::Vector, 3),
        type(LogicalTypeId::Vector, 3),
    };
    auto binding = catalog->bind_scalar("distance", valid);
    require(binding.has_value(), "custom constraint binding failed");
    require(
        binding->properties().semantic_tag == function::FunctionSemanticTag::VectorL2Distance,
        "semantic tag was not retained"
    );

    const std::vector<LogicalType> invalid {
        type(LogicalTypeId::Vector, 3),
        type(LogicalTypeId::Vector, 4),
    };
    auto rejected = catalog->bind_scalar("distance", invalid);
    require(!rejected.has_value(), "custom constraint should reject mismatched vectors");
    require(
        rejected.error().is(function::FunctionErrorCode::ConstraintViolation),
        "constraint error mismatch"
    );
}

void test_binding_survives_catalog_destruction()
{
    function::BoundScalarFunction binding = [&] {
        function::FunctionCatalogBuilder builder;
        require(builder.register_scalar(
            "constant",
            function::ScalarFunctionOverload {
                .parameters = {},
                .return_type = type(LogicalTypeId::Double),
                .bind = nullptr,
                .evaluate = evaluate_null,
                .properties = {},
            }
        ).has_value(), "constant registration failed");
        auto catalog = std::move(builder).build();
        require(catalog.has_value(), "constant catalog build failed");
        auto result = catalog->bind_scalar("constant", std::span<const LogicalType> {});
        require(result.has_value(), "constant binding failed");
        return std::move(*result);
    }();
    require(binding.evaluate({}, {}).has_value(), "binding did not survive catalog destruction");
}

} // namespace

int main()
{
    try {
        test_custom_bind_constraints();
        test_binding_survives_catalog_destruction();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
    return 0;
}
