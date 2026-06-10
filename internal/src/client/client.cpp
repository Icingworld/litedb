#include "client/client.hpp"

#include <system_error>
#include <utility>

#include "protocol/message.hpp"

namespace litedb::client
{

Client::Client(asio::io_context & io)
    : socket_(io)
{
}

asio::awaitable<std::expected<void, ClientError>> Client::connect(std::string_view host, std::uint16_t port)
{
    std::error_code error;
    const auto address = asio::ip::make_address(std::string(host), error);
    if (error) {
        co_return std::unexpected(ClientError {
            .code = ClientErrorCode::NetworkError,
            .message = error.message(),
        });
    }

    co_await socket_.async_connect(
        asio::ip::tcp::endpoint {address, port},
        asio::redirect_error(asio::use_awaitable, error)
    );
    if (error) {
        co_return std::unexpected(ClientError {
            .code = ClientErrorCode::NetworkError,
            .message = error.message(),
        });
    }

    co_return std::expected<void, ClientError> {};
}

asio::awaitable<std::expected<void, ClientError>> Client::ping()
{
    protocol::Frame request {
        .header = protocol::FrameHeader {
            .payload_size = 0,
            .version = protocol::ProtocolVersion,
            .kind = protocol::MessageKind::PingRequest,
            .request_id = next_request_id(),
        },
        .payload = {},
    };

    auto response = co_await roundtrip(std::move(request));
    if (!response.has_value()) {
        co_return std::unexpected(response.error());
    }
    if (response->header.kind != protocol::MessageKind::PongResponse) {
        co_return std::unexpected(ClientError {
            .code = ClientErrorCode::UnexpectedResponse,
            .message = "expected pong response",
        });
    }

    co_return std::expected<void, ClientError> {};
}

asio::awaitable<std::expected<core::executor::ExecutionResult, ClientError>> Client::execute_sql(std::string_view sql)
{
    protocol::Frame request {
        .header = protocol::FrameHeader {
            .payload_size = 0,
            .version = protocol::ProtocolVersion,
            .kind = protocol::MessageKind::ExecuteSqlRequest,
            .request_id = next_request_id(),
        },
        .payload = protocol::encode_execute_sql_request(sql),
    };

    auto response = co_await roundtrip(std::move(request));
    if (!response.has_value()) {
        co_return std::unexpected(response.error());
    }
    if (response->header.kind == protocol::MessageKind::ErrorResponse) {
        auto error = protocol::decode_error_response(response->payload);
        if (!error.has_value()) {
            co_return std::unexpected(from_protocol_error(error.error()));
        }
        co_return std::unexpected(ClientError {
            .code = ClientErrorCode::ServerError,
            .server_code = error->code,
            .message = std::move(error->message),
        });
    }
    if (response->header.kind != protocol::MessageKind::ExecuteSqlResponse) {
        co_return std::unexpected(ClientError {
            .code = ClientErrorCode::UnexpectedResponse,
            .message = "expected execute SQL response",
        });
    }

    auto result = protocol::decode_execute_sql_response(response->payload);
    if (!result.has_value()) {
        co_return std::unexpected(from_protocol_error(result.error()));
    }

    co_return std::move(result.value());
}

void Client::close()
{
    std::error_code ignored;
    socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ignored);
    socket_.close(ignored);
}

std::uint64_t Client::next_request_id() noexcept
{
    return next_request_id_++;
}

ClientError Client::from_network_error(const net::NetworkError & error) const
{
    return ClientError {
        .code = ClientErrorCode::NetworkError,
        .message = error.message,
    };
}

ClientError Client::from_protocol_error(const protocol::ProtocolError & error) const
{
    return ClientError {
        .code = ClientErrorCode::ProtocolError,
        .message = error.message,
    };
}

asio::awaitable<std::expected<protocol::Frame, ClientError>> Client::roundtrip(protocol::Frame frame)
{
    const auto request_id = frame.header.request_id;
    auto written = co_await net::async_write_frame(socket_, frame);
    if (!written.has_value()) {
        co_return std::unexpected(from_network_error(written.error()));
    }

    auto response = co_await net::async_read_frame(socket_);
    if (!response.has_value()) {
        co_return std::unexpected(from_network_error(response.error()));
    }
    if (response->header.request_id != request_id) {
        co_return std::unexpected(ClientError {
            .code = ClientErrorCode::UnexpectedResponse,
            .message = "response request id mismatch",
        });
    }

    co_return std::move(response.value());
}

} // namespace litedb::client
