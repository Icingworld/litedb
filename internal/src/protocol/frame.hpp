#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "protocol/constants.hpp"

namespace litedb::protocol
{

// 消息类型
enum class MessageKind : std::uint16_t
{
    // 连接管理
    HelloRequest = 0x0001,
    HelloResponse = 0x0002,
    CloseRequest = 0x0003,

    // SQL 执行
    ExecuteSqlRequest = 0x0100,
    ExecuteSqlResponse = 0x0101,
    CancelRequest = 0x0102,

    // 心跳管理
    PingRequest = 0x0200,
    PongResponse = 0x0201,

    // 错误响应
    ErrorResponse = 0xffff,
};

// 固定帧头
struct FrameHeader
{
    std::uint16_t version {ProtocolVersion}; // 协议版本
    MessageKind kind {MessageKind::ErrorResponse}; // 消息类型
    std::uint16_t flags {0}; // 标志
    std::uint64_t request_id {0}; // 请求 ID
};

// 线帧头
struct WireHeader
{
    FrameHeader header; // 帧头
    std::uint32_t frame_size {0}; // 帧大小
};

// 帧
struct Frame
{
    FrameHeader header; // 帧头
    std::vector<std::byte> payload; // 负载数据
};

} // namespace litedb::protocol
