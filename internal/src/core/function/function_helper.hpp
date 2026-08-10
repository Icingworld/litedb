#pragma once

#include <string_view>

#include "core/function/function_error.hpp"

namespace litedb::core::function
{

// 创建函数错误
[[nodiscard]]
FunctionError make_error(FunctionErrorCode code, std::string_view message);

} // namespace litedb::core::function
