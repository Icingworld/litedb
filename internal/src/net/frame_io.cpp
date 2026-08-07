#include "net/frame_io.hpp"

#include <array>
#include <cstddef>
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
        NetworkErrorCode::IoError,
        error.message(),
        NetworkErrorContext {.native_code = error.value()},
    };
}

} // namespace

asio::awaitable<std::expected<protocol::Frame, NetworkError>> async_read_frame(
    TcpSocket & socket,
    std::size_t max_frame_size
)
{
    std::array<std::byte, protocol::FrameHeaderSize> header_bytes {};
    std::error_code error;
    co_await asio::async_read(socket, asio::buffer(header_bytes), asio::redirect_error(asio::use_awaitable, error));
    if (error) {
        co_return std::unexpected(from_io_error(error));
    }

    auto header = protocol::decode_frame_header(header_bytes);
    if (!header.has_value()) {
        co_return std::unexpected(std::move(header.error()));
    }
    auto frame_size = header->frame_size;
    if (frame_size > max_frame_size || frame_size > protocol::MaxFrameSize) {
        co_return std::unexpected(NetworkError {
            NetworkErrorCode::FrameTooLarge,
            "frame exceeds maximum size",
        });
    }

    const auto payload_size = static_cast<std::size_t>(frame_size) - protocol::FrameHeaderSize;
    std::vector<std::byte> payload(payload_size);
    if (!payload.empty()) {
        co_await asio::async_read(socket, asio::buffer(payload), asio::redirect_error(asio::use_awaitable, error));
        if (error) {
            co_return std::unexpected(from_io_error(error));
        }
    }

    co_return protocol::Frame {
        .header = header->header,
        .payload = std::move(payload),
    };
}

asio::awaitable<std::expected<void, NetworkError>> async_write_frame(TcpSocket & socket, const protocol::Frame & frame)
{
    auto bytes = protocol::encode_frame(frame);
    if (!bytes.has_value()) {
        co_return std::unexpected(std::move(bytes.error()));
    }
    std::error_code error;
    co_await asio::async_write(socket, asio::buffer(*bytes), asio::redirect_error(asio::use_awaitable, error));
    if (error) {
        co_return std::unexpected(from_io_error(error));
    }
    co_return std::expected<void, NetworkError> {};
}

} // namespace litedb::net
