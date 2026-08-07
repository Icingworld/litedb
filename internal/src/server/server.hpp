#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>

#include <asio.hpp>

#include "core/database/database_engine.hpp"
#include "net/frame_io.hpp"
#include "protocol/message.hpp"

namespace litedb::server
{

struct ServerConfig
{
    std::string host {"127.0.0.1"};
    std::uint16_t port {0};
    std::size_t max_frame_size {protocol::MaxFrameSize};
    protocol::ProtocolDecodeLimits decode_limits {};
};

class Server
{
public:
    Server(asio::io_context & io, ServerConfig config, std::shared_ptr<core::database::DatabaseEngine> engine);

    Server(const Server &) = delete;
    Server & operator=(const Server &) = delete;

    [[nodiscard]]
    std::uint16_t port() const;

    void close();

    [[nodiscard]]
    asio::awaitable<void> listen();

private:
    [[nodiscard]]
    asio::awaitable<void> handle_connection(net::TcpSocket socket);

    [[nodiscard]]
    std::expected<protocol::Frame, protocol::ProtocolError> make_error_response(
        std::uint64_t request_id,
        std::uint16_t code,
        std::string message
    ) const;

    asio::ip::tcp::acceptor acceptor_;
    ServerConfig config_;
    std::shared_ptr<core::database::DatabaseEngine> engine_;
};

} // namespace litedb::server
