#include "core/function/function_catalog.hpp"

#include <algorithm>
#include <limits>
#include <optional>
#include <utility>

#include "core/common/identifier.hpp"
#include "core/common/type_rules.hpp"
#include "core/function/function_helper.hpp"

namespace litedb::core::function
{

namespace
{

/**
 * @brief 判断两个函数参数是否相同
 * @param left 左侧函数参数
 * @param right 右侧函数参数
 * @return 是否相同
 */
[[nodiscard]]
bool same_parameters(const FunctionParameters & left, const FunctionParameters & right)
{
    if (left.fixed.size() != right.fixed.size() ||
        left.variadic.has_value() != right.variadic.has_value()) {
        return false;
    }
    for (std::size_t index = 0; index < left.fixed.size(); ++index) {
        if (!common::same_type(left.fixed[index], right.fixed[index])) {
            return false;
        }
    }
    return !left.variadic.has_value() || common::same_type(*left.variadic, *right.variadic);
}

/**
 * @brief 获取函数参数
 * @param parameters 函数参数
 * @param index 参数索引
 * @return 参数
 */
[[nodiscard]]
const common::LogicalType & parameter_at(const FunctionParameters & parameters, std::size_t index)
{
    if (index < parameters.fixed.size()) {
        return parameters.fixed[index];
    }
    return *parameters.variadic;
}

/**
 * @brief 判断参数类型是否有效
 * @param type 参数类型
 * @return 是否有效
 */
[[nodiscard]]
bool valid_parameter_type(const common::LogicalType & type)
{
    if (type.id == common::LogicalTypeId::Null) {
        return false;
    }
    return type.id == common::LogicalTypeId::Vector || !type.parameter.has_value();
}

/**
 * @brief 判断绑定类型是否有效
 * @param source 源类型
 * @param target 目标类型
 * @return 是否有效
 */
[[nodiscard]]
bool valid_binding_type(const common::LogicalType & source, const common::LogicalType & target)
{
    return common::implicit_cast_cost(source, target).has_value();
}

} // namespace

std::expected<void, FunctionError>
FunctionCatalogBuilder::register_scalar(std::string_view name, ScalarFunctionOverload overload)
{
    // 函数名称不能为空，执行函数不能为空
    const auto key = common::normalize_identifier(name);
    if (key.empty() || overload.evaluate == nullptr) {
        return std::unexpected(make_error(
            FunctionErrorCode::InvalidDefinition,
            "Scalar function name and evaluator must be specified"
        ));
    }

    // 对于固定参数部分，逐个验证是否合法
    // 对于可变参数部分，验证第一个参数类型，因为可变参数要求可变部分类型一致
    if (std::ranges::any_of(
            overload.parameters.fixed,
            [](const auto & type) {
                return !valid_parameter_type(type);
            }
        ) ||
        (overload.parameters.variadic.has_value() &&
         !valid_parameter_type(*overload.parameters.variadic))) {
        return std::unexpected(make_error(
            FunctionErrorCode::InvalidDefinition,
            "Variadic scalar function parameter cannot be NULL"
        ));
    }

    // 检查是否存在重复的重载
    auto & overloads = functions_[key];
    for (const auto & existing : overloads) {
        if (same_parameters(existing->parameters, overload.parameters)) {
            return std::unexpected(make_error(
                FunctionErrorCode::DuplicateOverload,
                "Scalar function overload is already registered: " + key
            ));
        }
    }

    // 添加新的重载
    overloads.push_back(std::make_shared<const ScalarFunctionOverload>(std::move(overload)));

    return {};
}

std::expected<FunctionCatalog, FunctionError> FunctionCatalogBuilder::build() &&
{
    return FunctionCatalog {std::move(functions_)};
}

FunctionCatalog::FunctionCatalog(
    std::unordered_map<std::string, std::vector<std::shared_ptr<const ScalarFunctionOverload>>>
        functions
)
    : functions_(std::move(functions))
{}

bool FunctionCatalog::contains(std::string_view name) const
{
    return functions_.contains(common::normalize_identifier(name));
}

std::expected<BoundScalarFunction, FunctionError> FunctionCatalog::bind_scalar(
    std::string_view name,
    std::span<const common::LogicalType> argument_types
) const
{
    // 查找函数
    const auto found = functions_.find(common::normalize_identifier(name));
    if (found == functions_.end()) {
        return std::unexpected(make_error(
            FunctionErrorCode::FunctionNotFound,
            "Unknown scalar function: " + std::string(name)
        ));
    }

    // 寻找最佳匹配重载

    std::optional<BoundScalarFunction> best;
    std::size_t best_cost = std::numeric_limits<std::size_t>::max();
    std::size_t best_count = 0;
    std::optional<FunctionError> constraint_error;

    for (const auto & overload : found->second) {
        const auto & parameters = overload->parameters;

        if (argument_types.size() < parameters.fixed.size() ||
            (!parameters.variadic.has_value() &&
             argument_types.size() != parameters.fixed.size())) {
            // 参数数量不匹配，跳过
            continue;
        }

        // 逐个计算类型转换的代价
        std::vector<common::LogicalType> resolved_types;
        resolved_types.reserve(argument_types.size());
        std::size_t cost = 0;
        bool matches = true;
        for (std::size_t index = 0; index < argument_types.size(); ++index) {
            const auto & target = parameter_at(parameters, index);
            const auto conversion = common::implicit_cast_cost(argument_types[index], target);
            if (!conversion.has_value()) {
                matches = false;
                break;
            }
            cost += *conversion;
            if (target.id == common::LogicalTypeId::Vector && !target.parameter.has_value() &&
                argument_types[index].id == common::LogicalTypeId::Vector) {
                resolved_types.push_back(argument_types[index]);
            } else {
                resolved_types.push_back(target);
            }
        }
        if (!matches) {
            continue;
        }

        ScalarBindResult binding {
            .argument_types = resolved_types,
            .return_type = overload->return_type,
            .bind_data = nullptr,
        };
        if (overload->bind != nullptr) {
            auto custom = overload->bind(argument_types);
            if (!custom.has_value()) {
                constraint_error = std::move(custom.error());
                continue;
            }
            binding = std::move(*custom);
            if (binding.argument_types.size() != argument_types.size()) {
                constraint_error = make_error(
                    FunctionErrorCode::InvalidDefinition,
                    "Scalar function binding returned an invalid argument type list"
                );
                continue;
            }
            const auto valid_types = std::ranges::equal(
                argument_types,
                binding.argument_types,
                [](const auto & source, const auto & target) {
                    return valid_binding_type(source, target);
                }
            );
            if (!valid_types) {
                constraint_error = make_error(
                    FunctionErrorCode::InvalidDefinition,
                    "Scalar function binding returned incompatible argument types"
                );
                continue;
            }
        }

        BoundScalarFunction candidate {
            found->first,
            overload,
            std::move(binding.argument_types),
            binding.return_type,
            std::move(binding.bind_data),
            cost,
        };
        if (cost < best_cost) {
            best_cost = cost;
            best_count = 1;
            best = std::move(candidate);
        } else if (cost == best_cost) {
            ++best_count;
        }
    }

    if (!best.has_value()) {
        if (constraint_error.has_value()) {
            return std::unexpected(std::move(*constraint_error));
        }
        return std::unexpected(make_error(
            FunctionErrorCode::NoMatchingOverload,
            "No scalar function overload matches: " + std::string(name)
        ));
    }
    if (best_count != 1) {
        // 多个最佳匹配重载，函数调用歧义
        return std::unexpected(make_error(
            FunctionErrorCode::AmbiguousOverload,
            "Scalar function call is ambiguous: " + std::string(name)
        ));
    }
    return std::move(*best);
}

} // namespace litedb::core::function
