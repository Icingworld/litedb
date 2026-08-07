#pragma once

#include <cstddef>
#include <expected>
#include <optional>
#include <string>

#include <asio.hpp>

#include "protocol/codec.hpp"

namespace litedb::net
{

enum class NetworkErrorCode : std::uint8_t
{
    IoError,
    ProtocolError,
    FrameTooLarge,
};

struct NetworkErrorContext
{
    std::optional<int> native_code;
    std::optional<std::uint16_t> source_code;
};

using NetworkError = core::error::Error;

using TcpSocket = asio::ip::tcp::socket;

[[nodiscard]]
asio::awaitable<std::expected<protocol::Frame, NetworkError>> async_read_frame(
    TcpSocket & socket,
    std::size_t max_frame_size = protocol::MaxFrameSize
);

[[nodiscard]]
asio::awaitable<std::expected<void, NetworkError>> async_write_frame(TcpSocket & socket, const protocol::Frame & frame);

} // namespace litedb::net

namespace litedb::core::error
{
template <>
struct ErrorTraits<::litedb::net::NetworkErrorCode>
{
    static constexpr ErrorCategory category = ErrorCategory::Network;
};
} // namespace litedb::core::error
