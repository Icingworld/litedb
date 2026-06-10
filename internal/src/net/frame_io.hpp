#pragma once

#include <cstddef>
#include <expected>
#include <string>

#include <asio.hpp>

#include "protocol/message.hpp"

namespace litedb::net
{

enum class NetworkErrorCode
{
    IoError,
    ProtocolError,
    FrameTooLarge,
};

struct NetworkError
{
    NetworkErrorCode code;
    std::string message;
};

using TcpSocket = asio::ip::tcp::socket;

[[nodiscard]]
asio::awaitable<std::expected<protocol::Frame, NetworkError>> async_read_frame(
    TcpSocket & socket,
    std::size_t max_frame_size = protocol::DefaultMaxFrameSize
);

[[nodiscard]]
asio::awaitable<std::expected<void, NetworkError>> async_write_frame(TcpSocket & socket, const protocol::Frame & frame);

} // namespace litedb::net
