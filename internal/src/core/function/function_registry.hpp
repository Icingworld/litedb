#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "core/common/logical_type.hpp"
#include "core/function/function.hpp"
#include "core/function/function_signature.hpp"
#include "core/function/scalar_function.hpp"

namespace litedb::core::function
{

/**
 * @brief 函数绑定
 */
struct FunctionBinding
{
    std::shared_ptr<const ScalarFunction> function;     ///< 标量函数
    FunctionSignature signature;                        ///< 函数签名
};

/**
 * @brief 函数注册表
 */
class FunctionRegistry
{
public:
    /**
     * @brief 注册函数
     * @param function 函数
     */
    void register_function(std::shared_ptr<Function> function);

    /**
     * @brief 查找函数
     * @param name 函数名称
     * @return 函数
     */
    [[nodiscard]]
    std::shared_ptr<const Function> find(std::string_view name) const;

    /**
     * @brief 绑定标量函数
     * @param name 函数名称
     * @param argument_types 参数类型
     * @return 函数绑定
     */
    [[nodiscard]]
    std::optional<FunctionBinding> bind_scalar(
        std::string_view name,
        const std::vector<common::LogicalType> & argument_types
    ) const;

private:
    std::unordered_map<std::string, std::shared_ptr<Function>> functions_;     ///< 函数表
};


/**
 * @brief 规范化函数名称
 * @param name 函数名称
 * @return 规范化后的函数名称
 */
[[nodiscard]]
std::string normalize_function_name(std::string_view name);

} // namespace litedb::core::function
