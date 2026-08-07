#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "protocol/constants.hpp"

namespace litedb::protocol
{

/**
 * @brief 消息类型
 */
enum class MessageKind : std::uint16_t
{
    // 连接管理
    HelloRequest           = 0x0001,        // 握手请求
    HelloResponse          = 0x0002,        // 握手响应
    CloseRequest           = 0x0003,        // 关闭请求

    // SQL 执行
    ExecuteSqlRequest      = 0x0100,        // 执行 SQL 请求
    ExecuteSqlResponse     = 0x0101,        // 执行 SQL 响应
    CancelRequest          = 0x0102,        // 取消请求

    // 心跳管理
    PingRequest            = 0x0200,        // 心跳请求
    PongResponse           = 0x0201,        // 心跳响应

    // 错误响应
    ErrorResponse          = 0xffff,        // 错误响应
};

/**
 * @brief 固定帧头
 */
struct FrameHeader
{
    std::uint16_t version {ProtocolVersion};               // 协议版本
    MessageKind kind {MessageKind::ErrorResponse};         // 消息类型
    std::uint16_t flags {0};                               // 标志
    std::uint64_t request_id {0};                          // 请求 ID
};

/**
 * @brief 线帧头
 */
struct WireHeader
{
    FrameHeader header;                                     // 帧头
    std::uint32_t frame_size {0};                           // 帧大小
};

/**
 * @brief 帧
 */
struct Frame
{
    FrameHeader header;                                     // 帧头
    std::vector<std::byte> payload;                         // 负载数据
};

} // namespace litedb::protocol
