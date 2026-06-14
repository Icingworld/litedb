#pragma once

#include "core/function/function_registry.hpp"

namespace litedb::core::function::builtin
{

/**
 * @brief 注册向量函数
 * @param registry 函数注册表
 */
void register_vector_functions(FunctionRegistry & registry);

} // namespace litedb::core::function::builtin
