#pragma once

#include <cstdint>

#include "core/error/error.hpp"

namespace litedb::core::io
{

// IO 错误码
enum class IoErrorCode : std::uint8_t
{
    UnexpectedEof = 0,
    InvalidData = 1,
    ValueTooLarge = 2,
};

using IoError = error::Error;

} // namespace litedb::core::io

namespace litedb::core::error
{

template <>
struct ErrorTraits<io::IoErrorCode>
{
    static constexpr ErrorCategory category = ErrorCategory::Io;
};

} // namespace litedb::core::error
