#include "server/server.hpp"

#include <system_error>
#include <utility>

#include "core/database/session.hpp"
#include "core/database/session_error.hpp"
#include "protocol/message.hpp"

namespace litedb::server
{

namespace
{

[[nodiscard]]
std::uint16_t to_error_code(const core::error::Error & error) noexcept
{
    return static_cast<std::uint16_t>(error.code()) + 1U;
}

} // namespace

Server::Server(asio::io_context & io, ServerConfig config, std::shared_ptr<core::database::DatabaseEngine> engine)
    : acceptor_(io)
    , config_(std::move(config))
    , engine_(std::move(engine))
{
    const auto address = asio::ip::make_address(config_.host);
    asio::ip::tcp::endpoint endpoint {address, config_.port};
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen();
}

std::uint16_t Server::port() const
{
    return acceptor_.local_endpoint().port();
}

void Server::close()
{
    std::error_code ignored;
    // cancel 和 close 都会通过引用和返回值设置 error_code，显式赋值来避免未使用警告
    ignored = acceptor_.cancel(ignored);
    ignored = acceptor_.close(ignored);
}

asio::awaitable<void> Server::listen()
{
    for (;;) {
        std::error_code error;
        auto socket = co_await acceptor_.async_accept(asio::redirect_error(asio::use_awaitable, error));
        if (error == asio::error::operation_aborted || !acceptor_.is_open()) {
            co_return;
        }
        if (error) {
            continue;
        }

        asio::co_spawn(
            acceptor_.get_executor(),
            handle_connection(std::move(socket)),
            asio::detached
        );
    }
}

asio::awaitable<void> Server::handle_connection(net::TcpSocket socket)
{
    core::database::Session session {*engine_};

    for (;;) {
        auto frame = co_await net::async_read_frame(socket, config_.max_frame_size);
        if (!frame.has_value()) {
            co_return;
        }

        switch (frame->header.kind) {
        case protocol::MessageKind::PingRequest: {
            protocol::Frame pong {
                .header = protocol::FrameHeader {
                    .payload_size = 0,
                    .version = protocol::ProtocolVersion,
                    .kind = protocol::MessageKind::PongResponse,
                    .request_id = frame->header.request_id,
                },
                .payload = {},
            };
            auto written = co_await net::async_write_frame(socket, pong);
            if (!written.has_value()) {
                co_return;
            }
            break;
        }
        case protocol::MessageKind::ExecuteSqlRequest: {
            auto request = protocol::decode_execute_sql_request(frame->payload);
            if (!request.has_value()) {
                auto error = make_error_response(frame->header.request_id, 0, request.error().message());
                (void) co_await net::async_write_frame(socket, error);
                co_return;
            }

            auto executed = session.execute_sql(request->sql);
            if (!executed.has_value()) {
                auto error = make_error_response(
                    frame->header.request_id,
                    to_error_code(executed.error()),
                    executed.error().message()
                );
                auto written = co_await net::async_write_frame(socket, error);
                if (!written.has_value()) {
                    co_return;
                }
                break;
            }

            protocol::Frame response {
                .header = protocol::FrameHeader {
                    .payload_size = 0,
                    .version = protocol::ProtocolVersion,
                    .kind = protocol::MessageKind::ExecuteSqlResponse,
                    .request_id = frame->header.request_id,
                },
                .payload = protocol::encode_execute_sql_response(*executed),
            };
            auto written = co_await net::async_write_frame(socket, response);
            if (!written.has_value()) {
                co_return;
            }
            break;
        }
        default: {
            auto error = make_error_response(frame->header.request_id, 0, "unexpected request message kind");
            (void) co_await net::async_write_frame(socket, error);
            co_return;
        }
        }
    }
}

protocol::Frame Server::make_error_response(std::uint64_t request_id, std::uint16_t code, std::string message) const
{
    return protocol::Frame {
        .header = protocol::FrameHeader {
            .payload_size = 0,
            .version = protocol::ProtocolVersion,
            .kind = protocol::MessageKind::ErrorResponse,
            .request_id = request_id,
        },
        .payload = protocol::encode_error_response(protocol::ErrorResponse {
            .code = code,
            .message = std::move(message),
        }),
    };
}

} // namespace litedb::server
