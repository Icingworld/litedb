#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include <asio.hpp>

#include "core/engine/database_instance.hpp"
#include "net/frame_io.hpp"

namespace litedb::server
{

struct ServerConfig
{
    std::string host {"127.0.0.1"};
    std::uint16_t port {0};
    std::size_t max_frame_size {protocol::DefaultMaxFrameSize};
};

class Server
{
public:
    Server(asio::io_context & io, ServerConfig config, std::shared_ptr<core::engine::DatabaseInstance> instance);

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
    protocol::Frame make_error_response(std::uint64_t request_id, std::uint16_t code, std::string message) const;

    asio::ip::tcp::acceptor acceptor_;
    ServerConfig config_;
    std::shared_ptr<core::engine::DatabaseInstance> instance_;
};

} // namespace litedb::server
