#include "net/frame_io.hpp"

#include <array>
#include <cstddef>
#include <system_error>
#include <utility>
#include <vector>

#include "protocol/codec.hpp"

using TcpSocket = asio::ip::tcp::socket;

namespace litedb::net
{

asio::awaitable<std::expected<protocol::Frame, NetworkError>> async_read_frame(
    TcpSocket & socket,
    std::size_t max_frame_size
)
{
    // 构建大小为 32 字节的数组
    std::array<std::byte, protocol::FrameHeaderSize> header_bytes {};

    // 读取帧头
    std::error_code error;
    co_await asio::async_read(socket, asio::buffer(header_bytes), asio::redirect_error(asio::use_awaitable, error));
    if (error) {
        co_return std::unexpected(NetworkError {
            NetworkErrorCode::AsioError,
            error.message(),
            NetworkErrorContext {
                .error = error
            },
        });
    }

    // 解码帧头
    auto header = protocol::decode_frame_header(header_bytes);
    if (!header.has_value()) {
        co_return std::unexpected(std::move(header.error()));
    }
    // 获取帧大小
    auto frame_size = header->frame_size;
    if (frame_size > max_frame_size || frame_size > protocol::MaxFrameSize) {
        co_return std::unexpected(NetworkError {
            NetworkErrorCode::FrameTooLarge,
            "frame exceeds maximum size",
        });
    }

    // 读取负载
    const auto payload_size = static_cast<std::size_t>(frame_size) - protocol::FrameHeaderSize;
    std::vector<std::byte> payload(payload_size);
    if (!payload.empty()) {
        co_await asio::async_read(socket, asio::buffer(payload), asio::redirect_error(asio::use_awaitable, error));
        if (error) {
            co_return std::unexpected(std::move(error));
        }
    }

    co_return protocol::Frame {
        .header = header->header,
        .payload = std::move(payload),
    };
}

asio::awaitable<std::expected<void, NetworkError>> async_write_frame(
    TcpSocket & socket,
    const protocol::Frame & frame
)
{
    // 编码帧
    auto bytes = protocol::encode_frame(frame);
    if (!bytes.has_value()) {
        co_return std::unexpected(std::move(bytes.error()));
    }

    // 写入帧
    std::error_code error;
    co_await asio::async_write(socket, asio::buffer(*bytes), asio::redirect_error(asio::use_awaitable, error));
    if (error) {
        co_return std::unexpected(std::move(error));
    }

    co_return std::expected<void, NetworkError> {};
}

} // namespace litedb::net
