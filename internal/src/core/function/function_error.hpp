#pragma once

#include <cstdint>

#include "core/error/error.hpp"

namespace litedb::core::function
{

/**
 * @brief 函数错误代码
 */
enum class FunctionErrorCode : std::uint8_t
{
    InvalidArgument,        ///< 无效参数
    InvalidType,            ///< 无效类型
};

using FunctionError = error::Error;

} // namespace litedb::core::function

namespace litedb::core::error
{
template <>
struct ErrorTraits<function::FunctionErrorCode>
{
    static constexpr ErrorCategory category = ErrorCategory::Function;
};
} // namespace litedb::core::error
