#pragma once

#include <cstdint>

#include "core/error/error.hpp"

namespace litedb::core::function
{

/**
 * @brief 函数错误码
 */
enum class FunctionErrorCode : std::uint8_t
{
    FunctionNotFound = 0,               // 函数未找到
    NoMatchingOverload = 1,             // 没有匹配的函数重载
    AmbiguousOverload = 2,              // 函数重载歧义
    ConstraintViolation = 3,            // 约束违反
    InvalidDefinition = 4,              // 无效的函数定义
    DuplicateOverload = 5,              // 函数重载重复
    InvalidArgument = 6,                // 无效的函数参数
    InvalidType = 7,                    // 无效的函数类型
    ExecutionFailure = 8,               // 函数执行失败
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
