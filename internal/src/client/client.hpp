#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

#include <asio.hpp>

#include "core/executor/execution_result.hpp"
#include "net/frame_io.hpp"

namespace litedb::client
{

enum class ClientErrorCode
{
    NetworkError,
    ProtocolError,
    ServerError,
    UnexpectedResponse,
};

struct ClientError
{
    ClientErrorCode code;
    std::uint16_t server_code {0};
    std::string message;
};

class Client
{
public:
    explicit Client(asio::io_context & io);

    Client(const Client &) = delete;
    Client & operator=(const Client &) = delete;

    [[nodiscard]]
    asio::awaitable<std::expected<void, ClientError>> connect(std::string_view host, std::uint16_t port);

    [[nodiscard]]
    asio::awaitable<std::expected<void, ClientError>> ping();

    [[nodiscard]]
    asio::awaitable<std::expected<core::executor::ExecutionResult, ClientError>> execute_sql(std::string_view sql);

    void close();

private:
    [[nodiscard]]
    std::uint64_t next_request_id() noexcept;

    [[nodiscard]]
    ClientError from_network_error(const net::NetworkError & error) const;

    [[nodiscard]]
    ClientError from_protocol_error(const protocol::ProtocolError & error) const;

    [[nodiscard]]
    asio::awaitable<std::expected<protocol::Frame, ClientError>> roundtrip(protocol::Frame frame);

    net::TcpSocket socket_;
    std::uint64_t next_request_id_ {1};
};

} // namespace litedb::client
