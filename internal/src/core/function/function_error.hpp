#pragma once

#include <cstdint>

#include "core/error/error.hpp"

namespace litedb::core::function
{

// 函数错误码
enum class FunctionErrorCode : std::uint8_t
{
    FunctionNotFound = 0,
    NoMatchingOverload = 1,
    AmbiguousOverload = 2,
    ConstraintViolation = 3,
    InvalidDefinition = 4,
    DuplicateOverload = 5,
    InvalidArgument = 6,
    InvalidType = 7,
    ExecutionFailure = 8,
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
