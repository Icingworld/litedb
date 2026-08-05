#include "core/function/function_catalog.hpp"

#include <cstdint>
#include <expected>
#include <iostream>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
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

std::expected<Value, function::FunctionError> return_null(
    std::span<const Value>,
    const function::ScalarFunctionContext &,
    const function::FunctionBindData *
)
{
    return Value::null();
}

function::ScalarFunctionOverload overload(
    std::vector<LogicalType> fixed,
    LogicalType return_type,
    std::optional<LogicalType> variadic = std::nullopt
)
{
    return function::ScalarFunctionOverload {
        .parameters = function::FunctionParameters {
            .fixed = std::move(fixed),
            .variadic = std::move(variadic),
        },
        .return_type = return_type,
        .bind = nullptr,
        .evaluate = return_null,
        .properties = {},
    };
}

void test_registration_and_name_normalization()
{
    function::FunctionCatalogBuilder builder;
    require(builder.register_scalar(
        "Pick",
        overload({type(LogicalTypeId::Integer)}, type(LogicalTypeId::Integer))
    ).has_value(), "initial registration failed");
    require(builder.register_scalar(
        "PICK",
        overload({type(LogicalTypeId::Double)}, type(LogicalTypeId::Double))
    ).has_value(), "different overload registration failed");
    auto duplicate = builder.register_scalar(
        "pick",
        overload({type(LogicalTypeId::Integer)}, type(LogicalTypeId::Integer))
    );
    require(!duplicate.has_value(), "duplicate overload must fail");
    require(duplicate.error().is(function::FunctionErrorCode::DuplicateOverload), "duplicate error mismatch");

    auto catalog_result = std::move(builder).build();
    require(catalog_result.has_value(), "catalog build failed");
    require(catalog_result->contains("pIcK"), "function name normalization failed");
}

void test_invalid_definitions()
{
    function::FunctionCatalogBuilder builder;
    auto empty_name = builder.register_scalar(
        "",
        overload({}, type(LogicalTypeId::Integer))
    );
    require(!empty_name.has_value(), "empty function name should fail");
    require(empty_name.error().is(function::FunctionErrorCode::InvalidDefinition), "empty name error mismatch");

    auto no_evaluator = overload({}, type(LogicalTypeId::Integer));
    no_evaluator.evaluate = nullptr;
    auto missing_evaluator = builder.register_scalar("missing_eval", std::move(no_evaluator));
    require(!missing_evaluator.has_value(), "missing evaluator should fail");
    require(missing_evaluator.error().is(function::FunctionErrorCode::InvalidDefinition), "missing evaluator error mismatch");

    auto null_variadic = builder.register_scalar(
        "null_variadic",
        overload({}, type(LogicalTypeId::Integer), type(LogicalTypeId::Null))
    );
    require(!null_variadic.has_value(), "NULL variadic parameter should fail");
    require(null_variadic.error().is(function::FunctionErrorCode::InvalidDefinition), "variadic definition error mismatch");
}

void test_costed_overload_resolution()
{
    function::FunctionCatalogBuilder builder;
    require(builder.register_scalar(
        "pick",
        overload({type(LogicalTypeId::Integer)}, type(LogicalTypeId::Integer))
    ).has_value(), "integer overload registration failed");
    require(builder.register_scalar(
        "pick",
        overload({type(LogicalTypeId::Double)}, type(LogicalTypeId::Double))
    ).has_value(), "double overload registration failed");
    auto catalog = std::move(builder).build();
    require(catalog.has_value(), "catalog build failed");

    const std::vector<LogicalType> integer_types {type(LogicalTypeId::Integer)};
    auto exact = catalog->bind_scalar("pick", integer_types);
    require(exact.has_value(), "exact overload binding failed");
    require(exact->return_type().id == LogicalTypeId::Integer, "exact overload was not preferred");
    require(exact->match_cost() == 0, "exact overload cost mismatch");

    const std::vector<LogicalType> float_types {type(LogicalTypeId::Float)};
    auto widened = catalog->bind_scalar("pick", float_types);
    require(widened.has_value(), "widening overload binding failed");
    require(widened->return_type().id == LogicalTypeId::Double, "widening overload mismatch");

    const std::vector<LogicalType> null_types {type(LogicalTypeId::Null)};
    auto ambiguous = catalog->bind_scalar("pick", null_types);
    require(!ambiguous.has_value(), "NULL overload should be ambiguous");
    require(ambiguous.error().is(function::FunctionErrorCode::AmbiguousOverload), "ambiguity error mismatch");

    const std::vector<LogicalType> varchar_types {type(LogicalTypeId::Varchar)};
    auto no_match = catalog->bind_scalar("pick", varchar_types);
    require(!no_match.has_value(), "incompatible overload should fail");
    require(no_match.error().is(function::FunctionErrorCode::NoMatchingOverload), "no-match error mismatch");
}

void test_variadic_and_unknown_function()
{
    function::FunctionCatalogBuilder builder;
    require(builder.register_scalar(
        "concat_like",
        overload(
            {type(LogicalTypeId::Integer)},
            type(LogicalTypeId::Integer),
            type(LogicalTypeId::Double)
        )
    ).has_value(), "variadic registration failed");
    auto catalog = std::move(builder).build();
    require(catalog.has_value(), "catalog build failed");

    const std::vector<LogicalType> arguments {
        type(LogicalTypeId::Integer),
        type(LogicalTypeId::Float),
        type(LogicalTypeId::Double),
    };
    auto binding = catalog->bind_scalar("concat_like", arguments);
    require(binding.has_value(), "variadic binding failed");
    require(binding->argument_types().size() == 3, "variadic argument count mismatch");

    const std::vector<LogicalType> too_few {type(LogicalTypeId::Double)};
    auto invalid_arity = catalog->bind_scalar("concat_like", too_few);
    require(!invalid_arity.has_value(), "invalid variadic arity should fail");

    auto missing = catalog->bind_scalar("missing", arguments);
    require(!missing.has_value(), "unknown function should fail");
    require(missing.error().is(function::FunctionErrorCode::FunctionNotFound), "unknown function error mismatch");
}

} // namespace

int main()
{
    try {
        test_registration_and_name_normalization();
        test_invalid_definitions();
        test_costed_overload_resolution();
        test_variadic_and_unknown_function();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
    return 0;
}
