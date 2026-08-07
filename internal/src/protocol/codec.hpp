#pragma once

#include <expected>
#include <span>
#include <vector>

#include "protocol/frame.hpp"
#include "protocol/protocol_error.hpp"

namespace litedb::protocol
{

/**
 * @brief 编码帧
 * @param frame 帧
 * @return 编码后的字节序列
 */
[[nodiscard]]
std::expected<std::vector<std::byte>, ProtocolError> encode_frame(
    const Frame & frame
);

/**
 * @brief 解码帧头
 * @param bytes 字节序列
 * @return 解码后的线帧头
 */
[[nodiscard]]
std::expected<WireHeader, ProtocolError> decode_frame_header(
    std::span<const std::byte> bytes
);

/**
 * @brief 解码完整帧
 * @param bytes 字节序列
 * @return 解码后的帧
 */
[[nodiscard]]
std::expected<Frame, ProtocolError> decode_frame(
    std::span<const std::byte> bytes
);

/**
 * @brief 判断消息类型是否已知
 * @param kind 消息类型
 * @return 是否已知
 */
[[nodiscard]]
bool is_known_message_kind(MessageKind kind) noexcept;

} // namespace litedb::protocol
