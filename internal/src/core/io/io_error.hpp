#pragma once

#include <cstdint>

#include "core/error/error.hpp"

namespace litedb::core::io
{

/**
 * @brief IO 错误码
 */
enum class IoErrorCode : std::uint8_t
{
    UnexpectedEof = 0,          ///< 意外到达文件末尾
    InvalidData = 1,            ///< 数据无效
    ValueTooLarge = 2,          ///< 值过大，无法编码
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
