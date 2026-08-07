#pragma once

#include <system_error>

#include "core/error/error.hpp"

namespace litedb::net
{

/**
 * @brief 网络错误码
 */
enum class NetworkErrorCode : std::uint8_t
{
    AsioError = 0,                  ///< ASIO 错误
    FrameTooLarge = 1,              ///< 帧太大
};

/**
 * @brief 网络错误上下文
 */
struct NetworkErrorContext
{
    std::error_code error;          ///< 系统错误码
};

using NetworkError = core::error::Error;

} // namespace litedb::net

namespace litedb::core::error
{

template <>
struct ErrorTraits<::litedb::net::NetworkErrorCode>
{
    static constexpr ErrorCategory category = ErrorCategory::Network;
};

} // namespace litedb::core::error