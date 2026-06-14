#pragma once

#include "core/function/function_registry.hpp"

namespace litedb::core::function::builtin
{

/**
 * @brief 注册内置函数
 * @param registry 函数注册表
 */
void register_builtin_functions(FunctionRegistry & registry);

/**
 * @brief 创建内置函数注册表
 * @return 内置函数注册表
 */
[[nodiscard]]
FunctionRegistry make_builtin_function_registry();

} // namespace litedb::core::function::builtin
