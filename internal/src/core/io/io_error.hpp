#pragma once

#include <string>

namespace litedb::core::io
{

/**
 * @brief IO 错误码
 */
enum class IoErrorCode
{
    UnexpectedEof,             ///< 意外到达文件末尾
    InvalidData,               ///< 数据无效
    ValueTooLarge,             ///< 值过大，无法编码
    FileSystemError,           ///< 文件系统错误
};

/**
 * @brief IO 错误
 */
struct IoError
{
    IoErrorCode code;           ///< 错误码
    std::string message;        ///< 错误消息
};

} // namespace litedb::core::io
