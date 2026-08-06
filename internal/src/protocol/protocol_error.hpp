#pragma once

#include <cstdint>

#include "core/error/error.hpp"

namespace litedb::protocol
{

/**
 * @brief 协议错误码
 */
enum class ProtocolErrorCode : std::uint8_t
{
    InvalidFrame = 0,            ///< 无效的帧
};

using ProtocolError = core::error::Error;

} // namespace litedb::protocol

namespace litedb::core::error
{

template <>
struct ErrorTraits<::litedb::protocol::ProtocolErrorCode>
{
    static constexpr ErrorCategory category = ErrorCategory::Protocol;
};

} // namespace litedb::core::error