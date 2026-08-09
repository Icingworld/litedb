#pragma once

#include <cstdint>

#include "core/error/error.hpp"

namespace litedb::protocol
{

// 协议错误码
enum class ProtocolErrorCode : std::uint8_t
{
    UnexpectedEnd = 0, // 意外结束
    InvalidMagic = 1, // 无效的魔数
    UnsupportedVersion = 2, // 不支持的版本
    InvalidHeaderSize = 3, // 无效的头部大小
    InvalidFrameSize = 4, // 无效的帧大小
    FrameTooLarge = 5, // 帧太大
    InvalidMessageKind = 6, // 无效的消息类型
    InvalidFlags = 7, // 无效的标志
    InvalidReservedField = 8, // 无效的保留字段
    InvalidPayload = 9, // 无效的负载
    ResourceLimitExceeded = 10, // 资源限制超出
    HandshakeRequired = 11, // 需要握手
    UnexpectedMessage = 12, // 意外的消息
    UnsupportedMessage = 13, // 不支持的消息
};

using ProtocolError = core::error::Error;

} // namespace litedb::protocol

namespace litedb::core::error
{

template <> struct ErrorTraits<::litedb::protocol::ProtocolErrorCode>
{
    static constexpr ErrorCategory category = ErrorCategory::Protocol;
};

} // namespace litedb::core::error
