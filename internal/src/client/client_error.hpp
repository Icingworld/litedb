#pragma once

#include <cstdint>
#include <system_error>

#include "core/error/error.hpp"

namespace litedb::client
{

/**
 * @brief 客户端错误码
 */
enum class ClientErrorCode : std::uint8_t
{
    NetworkError = 0,
    ProtocolError = 1,
    ServerError = 2,
    UnexpectedResponse = 3,
};

/**
 * @brief 客户端错误上下文
 */
struct ClientErrorContext
{
    std::uint16_t server_code {0};   // 服务端错误码
    std::error_code error;           // 系统错误码
};

using ClientError = core::error::Error;

} // namespace litedb::client

namespace litedb::core::error
{

template <>
struct ErrorTraits<::litedb::client::ClientErrorCode>
{
    static constexpr ErrorCategory category = ErrorCategory::Client;
};

} // namespace litedb::core::error
