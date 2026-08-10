#include "core/function/builtin/vector_functions.hpp"

#include <cmath>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <utility>

namespace litedb::core::function::builtin
{

namespace
{

using common::LogicalType;
using common::LogicalTypeId;

LogicalType type(LogicalTypeId id, std::optional<std::size_t> parameter = std::nullopt)
{
    return LogicalType {id, parameter};
}

FunctionError make_error(FunctionErrorCode code, std::string message)
{
    return FunctionError {code, std::move(message)};
}

std::expected<ScalarBindResult, FunctionError> bind_vector_distance(
    std::span<const LogicalType> argument_types
)
{
    if (argument_types.size() != 2 || argument_types[0].id != LogicalTypeId::Vector ||
        argument_types[1].id != LogicalTypeId::Vector) {
        return std::unexpected(make_error(
            FunctionErrorCode::ConstraintViolation,
            "Vector distance expects two VECTOR arguments"
        ));
    }
    if (argument_types[0].parameter.has_value() && argument_types[1].parameter.has_value() &&
        argument_types[0].parameter != argument_types[1].parameter) {
        return std::unexpected(make_error(
            FunctionErrorCode::ConstraintViolation,
            "Vector function arguments must have the same dimension"
        ));
    }
    return ScalarBindResult {
        .argument_types = {argument_types[0], argument_types[1]},
        .return_type = type(LogicalTypeId::Double),
        .bind_data = nullptr,
    };
}

using DistanceFn = double (*)(const common::VectorValue &, const common::VectorValue &);

std::expected<common::Value, FunctionError>
evaluate_vector_distance(std::span<const common::Value> arguments, DistanceFn distance)
{
    if (arguments.size() != 2) {
        return std::unexpected(
            make_error(FunctionErrorCode::InvalidArgument, "Vector distance expects two arguments")
        );
    }
    const auto * left = std::get_if<common::VectorValue>(&arguments[0].data());
    const auto * right = std::get_if<common::VectorValue>(&arguments[1].data());
    if (left == nullptr || right == nullptr) {
        return std::unexpected(
            make_error(FunctionErrorCode::InvalidType, "Vector distance expects VECTOR values")
        );
    }
    if (left->size() != right->size()) {
        return std::unexpected(
            make_error(FunctionErrorCode::InvalidArgument, "Vector dimensions must match")
        );
    }
    return common::Value {distance(*left, *right)};
}

double l2_distance_impl(const common::VectorValue & left, const common::VectorValue & right)
{
    double sum = 0.0;
    for (std::size_t index = 0; index < left.size(); ++index) {
        const double difference = left[index] - right[index];
        sum += difference * difference;
    }
    return std::sqrt(sum);
}

double inner_product_impl(const common::VectorValue & left, const common::VectorValue & right)
{
    double result = 0.0;
    for (std::size_t index = 0; index < left.size(); ++index) {
        result += left[index] * right[index];
    }
    return result;
}

double cosine_distance_impl(const common::VectorValue & left, const common::VectorValue & right)
{
    double dot = 0.0;
    double left_norm = 0.0;
    double right_norm = 0.0;
    for (std::size_t index = 0; index < left.size(); ++index) {
        dot += left[index] * right[index];
        left_norm += left[index] * left[index];
        right_norm += right[index] * right[index];
    }
    if (left_norm == 0.0 || right_norm == 0.0) {
        return 1.0;
    }
    return 1.0 - (dot / (std::sqrt(left_norm) * std::sqrt(right_norm)));
}

std::expected<common::Value, FunctionError> evaluate_l2(
    std::span<const common::Value> arguments,
    const ScalarFunctionContext &,
    const FunctionBindData *
)
{
    return evaluate_vector_distance(arguments, l2_distance_impl);
}

std::expected<common::Value, FunctionError> evaluate_inner_product(
    std::span<const common::Value> arguments,
    const ScalarFunctionContext &,
    const FunctionBindData *
)
{
    return evaluate_vector_distance(arguments, inner_product_impl);
}

std::expected<common::Value, FunctionError> evaluate_cosine(
    std::span<const common::Value> arguments,
    const ScalarFunctionContext &,
    const FunctionBindData *
)
{
    return evaluate_vector_distance(arguments, cosine_distance_impl);
}

ScalarFunctionOverload
overload(ScalarFunctionOverload::EvalFn evaluate, FunctionSemanticTag semantic_tag)
{
    return ScalarFunctionOverload {
        .parameters =
            FunctionParameters {
                .fixed = {type(LogicalTypeId::Vector), type(LogicalTypeId::Vector)},
                .variadic = std::nullopt,
            },
        .return_type = type(LogicalTypeId::Double),
        .bind = bind_vector_distance,
        .evaluate = evaluate,
        .properties = FunctionProperties {
            .volatility = FunctionVolatility::Immutable,
            .null_handling = FunctionNullHandling::PropagateNull,
            .has_side_effects = false,
            .semantic_tag = semantic_tag,
        },
    };
}

} // namespace

std::expected<void, FunctionError> register_vector_functions(FunctionCatalogBuilder & builder)
{
    if (auto result = builder.register_scalar(
            "l2_distance",
            overload(evaluate_l2, FunctionSemanticTag::VectorL2Distance)
        );
        !result.has_value()) {
        return std::unexpected(std::move(result.error()));
    }
    if (auto result = builder.register_scalar(
            "inner_product",
            overload(evaluate_inner_product, FunctionSemanticTag::VectorInnerProduct)
        );
        !result.has_value()) {
        return std::unexpected(std::move(result.error()));
    }
    return builder.register_scalar(
        "cosine_distance",
        overload(evaluate_cosine, FunctionSemanticTag::VectorCosineDistance)
    );
}

} // namespace litedb::core::function::builtin
