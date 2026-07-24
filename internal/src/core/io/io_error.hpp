#pragma once

#include <cstdint>
#include <optional>

#include "core/error/error.hpp"

namespace litedb::core::io
{

/**
 * @brief IO 错误码
 */
enum class IOErrorCode : std::uint8_t
{
    UnexpectedEof = 0,          ///< 意外到达文件末尾
    InvalidData = 1,            ///< 数据无效
    ValueTooLarge = 2,          ///< 值过大，无法编码
    FileSystemError = 3,        ///< 文件系统错误
};

/**
 * @brief IO 错误上下文
 */
struct IOErrorContext
{
    std::optional<std::uint16_t> source_code;    ///< 下层错误的编码值
};

using IOError = error::Error;

// 保留既有源码拼写；两者均为统一 Error，不保留旧结构体语义。
using IoErrorCode = IOErrorCode;
using IoError = IOError;

} // namespace litedb::core::io

namespace litedb::core::error
{

template <>
struct ErrorTraits<io::IOErrorCode>
{
    static constexpr ErrorCategory category = ErrorCategory::IO;
};

} // namespace litedb::core::error
