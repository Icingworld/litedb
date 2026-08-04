#include "core/function/builtin/vector_functions.hpp"

#include <cmath>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/function/scalar_function.hpp"

namespace litedb::core::function::builtin
{

namespace
{

using common::LogicalType;
using common::LogicalTypeId;

[[nodiscard]]
LogicalType type(LogicalTypeId id, std::optional<std::size_t> parameter = std::nullopt)
{
    return LogicalType {id, parameter};
}

[[nodiscard]]
FunctionError make_error(
    FunctionErrorCode code,
    std::string message
)
{
    return FunctionError {code, message};
}

[[nodiscard]]
std::expected<common::Value, FunctionError> vector_binary_distance(
    const std::vector<common::Value> & arguments,
    double (*distance)(const common::VectorValue &, const common::VectorValue &)
)
{
    if (arguments.size() != 2) {
        return std::unexpected(make_error(FunctionErrorCode::InvalidArgument, "Vector distance expects 2 arguments"));
    }
    if (arguments[0].is_null() || arguments[1].is_null()) {
        return common::Value::null();
    }

    const auto * left = std::get_if<common::VectorValue>(&arguments[0].data());
    const auto * right = std::get_if<common::VectorValue>(&arguments[1].data());
    if (left == nullptr || right == nullptr) {
        return std::unexpected(make_error(FunctionErrorCode::InvalidType, "Vector distance expects VECTOR arguments"));
    }
    if (left->size() != right->size()) {
        return std::unexpected(make_error(FunctionErrorCode::InvalidArgument, "Vector dimensions must match"));
    }

    return common::Value {distance(*left, *right)};
}

[[nodiscard]]
double l2_distance_impl(const common::VectorValue & left, const common::VectorValue & right)
{
    double sum = 0.0;
    for (std::size_t index = 0; index < left.size(); ++index) {
        const double diff = left[index] - right[index];
        sum += diff * diff;
    }
    return std::sqrt(sum);
}

[[nodiscard]]
double inner_product_impl(const common::VectorValue & left, const common::VectorValue & right)
{
    double result = 0.0;
    for (std::size_t index = 0; index < left.size(); ++index) {
        result += left[index] * right[index];
    }
    return result;
}

[[nodiscard]]
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

[[nodiscard]]
std::vector<FunctionSignature> vector_distance_signatures(std::string name)
{
    return {
        FunctionSignature {
            .name = std::move(name),
            .argument_types = {type(LogicalTypeId::Vector), type(LogicalTypeId::Vector)},
            .return_type = type(LogicalTypeId::Double),
        },
    };
}

} // namespace

void register_vector_functions(FunctionRegistry & registry)
{
    registry.register_function(std::make_shared<ScalarFunction>(
        "l2_distance",
        vector_distance_signatures("l2_distance"),
        [](const std::vector<common::Value> & arguments,
           const ScalarFunctionContext &) {
            return vector_binary_distance(arguments, l2_distance_impl);
        }
    ));
    registry.register_function(std::make_shared<ScalarFunction>(
        "inner_product",
        vector_distance_signatures("inner_product"),
        [](const std::vector<common::Value> & arguments,
           const ScalarFunctionContext &) {
            return vector_binary_distance(arguments, inner_product_impl);
        }
    ));
    registry.register_function(std::make_shared<ScalarFunction>(
        "cosine_distance",
        vector_distance_signatures("cosine_distance"),
        [](const std::vector<common::Value> & arguments,
           const ScalarFunctionContext &) {
            return vector_binary_distance(arguments, cosine_distance_impl);
        }
    ));
}

} // namespace litedb::core::function::builtin
