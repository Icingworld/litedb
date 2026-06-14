#include "core/function/function_registry.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

namespace litedb::core::function
{

namespace
{

/**
 * @brief 判断两个类型是否相同
 * @param left 左类型
 * @param right 右类型
 * @return 是否相同
 */
[[nodiscard]]
bool same_type(const common::LogicalType & left, const common::LogicalType & right)
{
    return left.id == right.id && left.parameter == right.parameter;
}

/**
 * @brief 判断类型是否为数字类型
 * @param type 类型
 * @return 是否为数字类型
 */
[[nodiscard]]
bool is_numeric(const common::LogicalType & type)
{
    return type.id == common::LogicalTypeId::Integer
        || type.id == common::LogicalTypeId::BigInt
        || type.id == common::LogicalTypeId::Float
        || type.id == common::LogicalTypeId::Double;
}

/**
 * @brief 判断类型是否可以转换
 * @param source 源类型
 * @param target 目标类型
 * @return 是否可以转换
 */
[[nodiscard]]
bool can_cast(const common::LogicalType & source, const common::LogicalType & target)
{
    if (source.id == common::LogicalTypeId::Null) {
        return true;
    }
    if (same_type(source, target)) {
        return true;
    }
    if (is_numeric(source) && is_numeric(target)) {
        return true;
    }
    if (source.id == common::LogicalTypeId::Vector && target.id == common::LogicalTypeId::Vector) {
        return !source.parameter.has_value()
            || !target.parameter.has_value()
            || source.parameter.value() == target.parameter.value();
    }
    return false;
}

/**
 * @brief 判断函数签名是否匹配
 * @param signature 函数签名
 * @param argument_types 参数类型
 * @return 是否匹配
 */
[[nodiscard]]
bool signature_matches(
    const FunctionSignature & signature,
    const std::vector<common::LogicalType> & argument_types
)
{
    if (!signature.variadic && signature.argument_types.size() != argument_types.size()) {
        return false;
    }
    if (signature.variadic && argument_types.size() < signature.argument_types.size()) {
        return false;
    }

    for (std::size_t index = 0; index < argument_types.size(); ++index) {
        const auto signature_index = std::min(index, signature.argument_types.size() - 1);
        if (!can_cast(argument_types[index], signature.argument_types[signature_index])) {
            return false;
        }
    }
    return true;
}

} // namespace

std::string normalize_function_name(std::string_view name)
{
    std::string normalized {name};
    std::ranges::transform(normalized, normalized.begin(), [](unsigned char ch) {
        // 将字符转换为小写
        return static_cast<char>(std::tolower(ch));
    });
    return normalized;
}

void FunctionRegistry::register_function(std::shared_ptr<Function> function)
{
    functions_[normalize_function_name(function->name())] = std::move(function);
}

std::shared_ptr<const Function> FunctionRegistry::find(std::string_view name) const
{
    const auto found = functions_.find(normalize_function_name(name));
    if (found == functions_.end()) {
        return nullptr;
    }
    return found->second;
}

std::optional<FunctionBinding> FunctionRegistry::bind_scalar(
    std::string_view name,
    const std::vector<common::LogicalType> & argument_types
) const
{
    auto function = find(name);
    if (function == nullptr || function->kind() != FunctionKind::Scalar) {
        return std::nullopt;
    }

    const auto scalar = std::static_pointer_cast<const ScalarFunction>(function);
    for (const auto & signature : scalar->signatures()) {
        if (signature_matches(signature, argument_types)) {
            return FunctionBinding {
                .function = scalar,
                .signature = signature,
            };
        }
    }
    return std::nullopt;
}

} // namespace litedb::core::function
