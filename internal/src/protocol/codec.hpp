#pragma once

#include <expected>
#include <span>
#include <vector>

#include "protocol/frame.hpp"
#include "protocol/protocol_error.hpp"

namespace litedb::protocol
{

// 编码帧
[[nodiscard]]
std::expected<std::vector<std::byte>, ProtocolError> encode_frame(const Frame & frame);

// 解码帧头
[[nodiscard]]
std::expected<WireHeader, ProtocolError> decode_frame_header(std::span<const std::byte> bytes);

// 解码完整帧
[[nodiscard]]
std::expected<Frame, ProtocolError> decode_frame(std::span<const std::byte> bytes);

// 判断消息类型是否已知
[[nodiscard]]
bool is_known_message_kind(MessageKind kind) noexcept;

} // namespace litedb::protocol
