#pragma once

#include <string>

namespace litedb::core::wal
{

/**
 * @brief WAL 错误码
 */
enum class WalErrorCode
{
    FileSystemError,       ///< 文件系统错误
    InvalidFormat,         ///< 格式错误
    UnsupportedVersion,    ///< 不支持的版本
    CorruptedRecord,       ///< 损坏的记录
    InvalidRecord,         ///< 无效的记录
    MissingTarget,         ///< 缺少目标
};

/**
 * @brief WAL 错误
 */
struct WalError
{
    WalErrorCode code;     ///< 错误码
    std::string message;   ///< 错误消息
};

/**
 * @brief 创建 WAL 错误
 * @param code 错误码
 * @param message 错误消息
 * @return WAL 错误
 */
[[nodiscard]]
inline WalError make_error(WalErrorCode code, std::string message)
{
    return {code, std::move(message)};
}

} // namespace litedb::core::wal
