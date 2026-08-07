#pragma once

#include <cstddef>
#include <expected>

#include <asio.hpp>

#include "net/net_error.hpp"
#include "protocol/constants.hpp"
#include "protocol/frame.hpp"

namespace litedb::net
{

/**
 * @brief 异步读取帧
 * @param socket 套接字
 * @param max_frame_size 最大帧大小
 * @return 帧
 */
[[nodiscard]]
asio::awaitable<std::expected<protocol::Frame, NetworkError>>
async_read_frame(
    asio::ip::tcp::socket & socket,
    std::size_t max_frame_size = protocol::MaxFrameSize
);

/**
 * @brief 异步写入帧
 * @param socket 套接字
 * @param frame 帧
 * @return 是否成功
 */
[[nodiscard]]
asio::awaitable<std::expected<void, NetworkError>>
async_write_frame(
    asio::ip::tcp::socket & socket,
    const protocol::Frame & frame
);

} // namespace litedb::net
