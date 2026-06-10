#include "net/frame_io.hpp"

#include <array>
#include <system_error>
#include <utility>
#include <vector>

namespace litedb::net
{

namespace
{

[[nodiscard]]
NetworkError from_io_error(const std::error_code & error)
{
    return NetworkError {
        .code = NetworkErrorCode::IoError,
        .message = error.message(),
    };
}

[[nodiscard]]
NetworkError from_protocol_error(protocol::ProtocolError error)
{
    return NetworkError {
        .code = NetworkErrorCode::ProtocolError,
        .message = std::move(error.message),
    };
}

} // namespace

asio::awaitable<std::expected<protocol::Frame, NetworkError>> async_read_frame(
    TcpSocket & socket,
    std::size_t max_frame_size
)
{
    std::array<std::uint8_t, protocol::FrameHeaderSize> header_bytes {};
    std::error_code error;
    co_await asio::async_read(socket, asio::buffer(header_bytes), asio::redirect_error(asio::use_awaitable, error));
    if (error) {
        co_return std::unexpected(from_io_error(error));
    }

    auto header = protocol::decode_frame_header(header_bytes.data(), header_bytes.size());
    if (!header.has_value()) {
        co_return std::unexpected(from_protocol_error(std::move(header.error())));
    }
    if (header->payload_size > max_frame_size) {
        co_return std::unexpected(NetworkError {
            .code = NetworkErrorCode::FrameTooLarge,
            .message = "frame payload exceeds maximum size",
        });
    }

    std::vector<std::uint8_t> payload(header->payload_size);
    if (!payload.empty()) {
        co_await asio::async_read(socket, asio::buffer(payload), asio::redirect_error(asio::use_awaitable, error));
        if (error) {
            co_return std::unexpected(from_io_error(error));
        }
    }

    co_return protocol::Frame {
        .header = header.value(),
        .payload = std::move(payload),
    };
}

asio::awaitable<std::expected<void, NetworkError>> async_write_frame(TcpSocket & socket, const protocol::Frame & frame)
{
    auto bytes = protocol::encode_frame(frame);
    std::error_code error;
    co_await asio::async_write(socket, asio::buffer(bytes), asio::redirect_error(asio::use_awaitable, error));
    if (error) {
        co_return std::unexpected(from_io_error(error));
    }
    co_return std::expected<void, NetworkError> {};
}

} // namespace litedb::net
