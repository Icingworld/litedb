#include "server/server.hpp"

#include <system_error>
#include <utility>

#include "core/common/logical_type.hpp"
#include "core/common/value.hpp"
#include "core/database/session.hpp"
#include "protocol/message.hpp"
#include "net/frame_io.hpp"

using TcpSocket = asio::ip::tcp::socket;

namespace litedb::server
{

namespace
{

protocol::Frame make_frame(
    protocol::MessageKind kind,
    std::uint64_t request_id,
    std::vector<std::byte> payload = {}
)
{
    return protocol::Frame {
        .header = protocol::FrameHeader {
            .version = protocol::ProtocolVersion,
            .kind = kind,
            .flags = 0,
            .request_id = request_id,
        },
        .payload = std::move(payload),
    };
}

std::uint16_t protocol_error_code(protocol::ProtocolErrorCode code) noexcept
{
    return protocol::ProtocolError {code, ""}.encode_code();
}

std::expected<protocol::LogicalType, protocol::ProtocolError> to_protocol_type(
    const core::common::LogicalType & type
)
{
    protocol::LogicalTypeId id;
    switch (type.id) {
    case core::common::LogicalTypeId::Null: id = protocol::LogicalTypeId::Null; break;
    case core::common::LogicalTypeId::Boolean: id = protocol::LogicalTypeId::Boolean; break;
    case core::common::LogicalTypeId::Integer: id = protocol::LogicalTypeId::Integer; break;
    case core::common::LogicalTypeId::BigInt: id = protocol::LogicalTypeId::BigInt; break;
    case core::common::LogicalTypeId::Float: id = protocol::LogicalTypeId::Float; break;
    case core::common::LogicalTypeId::Double: id = protocol::LogicalTypeId::Double; break;
    case core::common::LogicalTypeId::Varchar: id = protocol::LogicalTypeId::Varchar; break;
    case core::common::LogicalTypeId::Vector: id = protocol::LogicalTypeId::Vector; break;
    default:
        return std::unexpected(protocol::ProtocolError {
            protocol::ProtocolErrorCode::InvalidPayload,
            "invalid execution logical type",
        });
    }
    return protocol::LogicalType {
        .id = id,
        .parameter = type.parameter.has_value()
            ? std::optional<std::uint64_t> {static_cast<std::uint64_t>(*type.parameter)}
            : std::nullopt,
    };
}

protocol::Value to_protocol_value(const core::common::Value & value)
{
    return std::visit(
        [](const auto & data) -> protocol::Value {
            return protocol::Value {.data = data};
        },
        value.data()
    );
}

std::expected<protocol::ExecuteSqlResponse, protocol::ProtocolError> to_protocol_result(
    core::executor::ExecutionResult result
)
{
    protocol::ExecuteSqlResponse response;
    switch (result.kind) {
    case core::executor::ExecutionResultKind::Command:
        response.kind = protocol::ResultKind::Command;
        break;
    case core::executor::ExecutionResultKind::RowSet:
        response.kind = protocol::ResultKind::RowSet;
        break;
    case core::executor::ExecutionResultKind::UseDatabase:
        response.kind = protocol::ResultKind::UseDatabase;
        break;
    default:
        return std::unexpected(protocol::ProtocolError {
            protocol::ProtocolErrorCode::InvalidPayload,
            "invalid execution result kind",
        });
    }
    response.affected_rows = static_cast<std::uint64_t>(result.affected_rows);
    response.selected_database_name = std::move(result.selected_database_name);
    response.columns.reserve(result.columns.size());
    for (auto & column : result.columns) {
        auto type = to_protocol_type(column.type);
        if (!type) {
            return std::unexpected(std::move(type.error()));
        }
        response.columns.push_back(protocol::Column {
            .name = std::move(column.name),
            .type = *type,
        });
    }
    response.rows.reserve(result.rows.size());
    for (auto & row : result.rows) {
        protocol::Row target;
        target.values.reserve(row.values.size());
        for (const auto & value : row.values) {
            target.values.push_back(to_protocol_value(value));
        }
        response.rows.push_back(std::move(target));
    }
    return response;
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

asio::awaitable<void> Server::handle_connection(TcpSocket socket)
{
    core::database::Session session {*engine_};
    bool handshaken = false;

    for (;;) {
        auto frame = co_await net::async_read_frame(socket, config_.max_frame_size);
        if (!frame) {
            co_return;
        }

        if (!handshaken) {
            if (frame->header.kind != protocol::MessageKind::HelloRequest) {
                auto error = make_error_response(
                    frame->header.request_id,
                    protocol_error_code(protocol::ProtocolErrorCode::HandshakeRequired),
                    "hello handshake is required"
                );
                if (!error) {
                    co_return;
                }
                (void) co_await net::async_write_frame(socket, *error);
                co_return;
            }
            auto request = protocol::decode_hello_request(frame->payload, config_.decode_limits);
            if (!request || request->min_version > protocol::ProtocolVersion
                || request->max_version < protocol::ProtocolVersion) {
                const auto message = request ? "hello version range does not include protocol version 1"
                                             : request.error().message();
                const auto code = request ? protocol_error_code(protocol::ProtocolErrorCode::UnsupportedVersion)
                                           : request.error().encode_code();
                auto error = make_error_response(frame->header.request_id, code, message);
                if (!error) {
                    co_return;
                }
                (void) co_await net::async_write_frame(socket, *error);
                co_return;
            }
            auto payload = protocol::encode_hello_response(protocol::HelloResponse {});
            if (!payload) {
                co_return;
            }
            auto response = make_frame(
                protocol::MessageKind::HelloResponse,
                frame->header.request_id,
                std::move(*payload)
            );
            auto written = co_await net::async_write_frame(socket, response);
            if (!written) {
                co_return;
            }
            handshaken = true;
            continue;
        }

        switch (frame->header.kind) {
        case protocol::MessageKind::PingRequest: {
            if (!frame->payload.empty()) {
                auto error = make_error_response(
                    frame->header.request_id,
                    protocol_error_code(protocol::ProtocolErrorCode::InvalidPayload),
                    "ping request payload must be empty"
                );
                if (error) {
                    (void) co_await net::async_write_frame(socket, *error);
                }
                co_return;
            }
            auto written = co_await net::async_write_frame(
                socket,
                make_frame(protocol::MessageKind::PongResponse, frame->header.request_id)
            );
            if (!written) {
                co_return;
            }
            break;
        }
        case protocol::MessageKind::ExecuteSqlRequest: {
            auto request = protocol::decode_execute_sql_request(frame->payload, config_.decode_limits);
            if (!request) {
                auto error = make_error_response(
                    frame->header.request_id,
                    request.error().encode_code(),
                    request.error().message()
                );
                if (!error) {
                    co_return;
                }
                (void) co_await net::async_write_frame(socket, *error);
                co_return;
            }

            auto executed = session.execute_sql(request->sql);
            if (!executed) {
                auto error = make_error_response(
                    frame->header.request_id,
                    executed.error().encode_code(),
                    executed.error().message()
                );
                if (!error) {
                    co_return;
                }
                auto written = co_await net::async_write_frame(socket, *error);
                if (!written) {
                    co_return;
                }
                break;
            }

            auto response_payload = to_protocol_result(std::move(*executed));
            if (!response_payload) {
                auto error = make_error_response(
                    frame->header.request_id,
                    response_payload.error().encode_code(),
                    response_payload.error().message()
                );
                if (!error) {
                    co_return;
                }
                (void) co_await net::async_write_frame(socket, *error);
                co_return;
            }
            auto encoded = protocol::encode_execute_sql_response(*response_payload);
            if (!encoded) {
                auto error = make_error_response(
                    frame->header.request_id,
                    encoded.error().encode_code(),
                    encoded.error().message()
                );
                if (!error) {
                    co_return;
                }
                (void) co_await net::async_write_frame(socket, *error);
                co_return;
            }
            auto written = co_await net::async_write_frame(
                socket,
                make_frame(
                    protocol::MessageKind::ExecuteSqlResponse,
                    frame->header.request_id,
                    std::move(*encoded)
                )
            );
            if (!written) {
                co_return;
            }
            break;
        }
        case protocol::MessageKind::CancelRequest: {
            auto error = make_error_response(
                frame->header.request_id,
                protocol_error_code(protocol::ProtocolErrorCode::UnsupportedMessage),
                "cancel is not supported"
            );
            if (!error) {
                co_return;
            }
            auto written = co_await net::async_write_frame(socket, *error);
            if (!written) {
                co_return;
            }
            break;
        }
        case protocol::MessageKind::CloseRequest: {
            if (!frame->payload.empty()) {
                auto error = make_error_response(
                    frame->header.request_id,
                    protocol_error_code(protocol::ProtocolErrorCode::InvalidPayload),
                    "close request payload must be empty"
                );
                if (error) {
                    (void) co_await net::async_write_frame(socket, *error);
                }
            }
            co_return;
        }
        case protocol::MessageKind::HelloRequest:
        case protocol::MessageKind::HelloResponse:
        case protocol::MessageKind::ExecuteSqlResponse:
        case protocol::MessageKind::PongResponse:
        case protocol::MessageKind::ErrorResponse: {
            auto error = make_error_response(
                frame->header.request_id,
                protocol_error_code(protocol::ProtocolErrorCode::UnexpectedMessage),
                "unexpected message kind"
            );
            if (!error) {
                co_return;
            }
            (void) co_await net::async_write_frame(socket, *error);
            co_return;
        }
        }
    }
}

std::expected<protocol::Frame, protocol::ProtocolError> Server::make_error_response(
    std::uint64_t request_id,
    std::uint16_t code,
    std::string message
) const
{
    auto payload = protocol::encode_error_response(protocol::ErrorResponse {
        .code = code,
        .message = std::move(message),
    });
    if (!payload) {
        return std::unexpected(std::move(payload.error()));
    }
    return make_frame(protocol::MessageKind::ErrorResponse, request_id, std::move(*payload));
}

} // namespace litedb::server
