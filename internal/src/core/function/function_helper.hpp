#pragma once

#include <string_view>

#include "core/function/function_error.hpp"

namespace litedb::core::function
{

/**
 * @brief 创建函数错误
 * @param code 错误码
 * @param message 错误消息
 * @return 函数错误
 */
[[nodiscard]]
FunctionError make_error(FunctionErrorCode code, std::string_view message);

} // namespace litedb::core::function
