#include "client/client.hpp"

#include <limits>
#include <system_error>
#include <type_traits>
#include <utility>

namespace litedb::client
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

ClientError invalid_result(std::string_view message)
{
    return ClientError {ClientErrorCode::ProtocolError, message};
}

std::expected<core::common::LogicalType, ClientError> to_core_type(const protocol::LogicalType & type)
{
    core::common::LogicalTypeId id;
    switch (type.id) {
    case protocol::LogicalTypeId::Null: id = core::common::LogicalTypeId::Null; break;
    case protocol::LogicalTypeId::Boolean: id = core::common::LogicalTypeId::Boolean; break;
    case protocol::LogicalTypeId::Integer: id = core::common::LogicalTypeId::Integer; break;
    case protocol::LogicalTypeId::BigInt: id = core::common::LogicalTypeId::BigInt; break;
    case protocol::LogicalTypeId::Float: id = core::common::LogicalTypeId::Float; break;
    case protocol::LogicalTypeId::Double: id = core::common::LogicalTypeId::Double; break;
    case protocol::LogicalTypeId::Varchar: id = core::common::LogicalTypeId::Varchar; break;
    case protocol::LogicalTypeId::Vector: id = core::common::LogicalTypeId::Vector; break;
    default:
        return std::unexpected(invalid_result("invalid protocol logical type"));
    }
    if (type.parameter.has_value()
        && *type.parameter > std::numeric_limits<std::size_t>::max()) {
        return std::unexpected(invalid_result("logical type parameter exceeds the local size limit"));
    }
    return core::common::LogicalType {
        .id = id,
        .parameter = type.parameter.has_value()
            ? std::optional<std::size_t> {static_cast<std::size_t>(*type.parameter)}
            : std::nullopt,
    };
}

core::common::Value to_core_value(const protocol::Value & value)
{
    return std::visit(
        [](const auto & data) -> core::common::Value {
            using T = std::decay_t<decltype(data)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                return core::common::Value::null();
            } else {
                return core::common::Value {core::common::ValueData {data}};
            }
        },
        value.data
    );
}

std::expected<core::executor::ExecutionResult, ClientError> to_core_result(protocol::ExecuteSqlResponse response)
{
    core::executor::ExecutionResult result;
    switch (response.kind) {
    case protocol::ResultKind::Command:
        result.kind = core::executor::ExecutionResultKind::Command;
        break;
    case protocol::ResultKind::RowSet:
        result.kind = core::executor::ExecutionResultKind::RowSet;
        break;
    case protocol::ResultKind::UseDatabase:
        result.kind = core::executor::ExecutionResultKind::UseDatabase;
        break;
    default:
        return std::unexpected(invalid_result("invalid protocol result kind"));
    }
    if (response.affected_rows > std::numeric_limits<std::size_t>::max()) {
        return std::unexpected(invalid_result("affected row count exceeds the local size limit"));
    }
    result.affected_rows = static_cast<std::size_t>(response.affected_rows);
    result.selected_database_name = std::move(response.selected_database_name);
    result.columns.reserve(response.columns.size());
    for (auto & column : response.columns) {
        auto type = to_core_type(column.type);
        if (!type) {
            return std::unexpected(std::move(type.error()));
        }
        result.columns.push_back(core::executor::ExecutionColumn {
            .name = std::move(column.name),
            .type = *type,
        });
    }
    result.rows.reserve(response.rows.size());
    for (auto & row : response.rows) {
        core::executor::ExecutionRow target;
        target.values.reserve(row.values.size());
        for (const auto & value : row.values) {
            target.values.push_back(to_core_value(value));
        }
        result.rows.push_back(std::move(target));
    }
    return result;
}

} // namespace

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
            ClientErrorCode::NetworkError,
            error.message(),
            ClientErrorContext {.native_code = error.value()},
        });
    }

    co_await socket_.async_connect(
        asio::ip::tcp::endpoint {address, port},
        asio::redirect_error(asio::use_awaitable, error)
    );
    if (error) {
        co_return std::unexpected(ClientError {
            ClientErrorCode::NetworkError,
            error.message(),
            ClientErrorContext {.native_code = error.value()},
        });
    }

    auto payload = protocol::encode_hello_request(protocol::HelloRequest {});
    if (!payload) {
        co_return std::unexpected(std::move(payload.error()));
    }
    auto response = co_await roundtrip(make_frame(
        protocol::MessageKind::HelloRequest,
        next_request_id(),
        std::move(*payload)
    ));
    if (!response) {
        co_return std::unexpected(std::move(response.error()));
    }
    if (response->header.kind == protocol::MessageKind::ErrorResponse) {
        auto decoded = protocol::decode_error_response(response->payload);
        if (!decoded) {
            co_return std::unexpected(std::move(decoded.error()));
        }
        co_return std::unexpected(ClientError {
            ClientErrorCode::ServerError,
            decoded->message,
            ClientErrorContext {.server_code = decoded->code},
        });
    }
    if (response->header.kind != protocol::MessageKind::HelloResponse) {
        co_return std::unexpected(ClientError {
            ClientErrorCode::UnexpectedResponse,
            "expected hello response",
        });
    }
    auto hello = protocol::decode_hello_response(response->payload);
    if (!hello) {
        co_return std::unexpected(std::move(hello.error()));
    }
    co_return std::expected<void, ClientError> {};
}

asio::awaitable<std::expected<void, ClientError>> Client::ping()
{
    auto response = co_await roundtrip(make_frame(
        protocol::MessageKind::PingRequest,
        next_request_id()
    ));
    if (!response) {
        co_return std::unexpected(std::move(response.error()));
    }
    if (response->header.kind != protocol::MessageKind::PongResponse || !response->payload.empty()) {
        co_return std::unexpected(ClientError {
            ClientErrorCode::UnexpectedResponse,
            "expected empty pong response",
        });
    }
    co_return std::expected<void, ClientError> {};
}

asio::awaitable<std::expected<core::executor::ExecutionResult, ClientError>> Client::execute_sql(std::string_view sql)
{
    auto payload = protocol::encode_execute_sql_request(sql);
    if (!payload) {
        co_return std::unexpected(std::move(payload.error()));
    }
    auto response = co_await roundtrip(make_frame(
        protocol::MessageKind::ExecuteSqlRequest,
        next_request_id(),
        std::move(*payload)
    ));
    if (!response) {
        co_return std::unexpected(std::move(response.error()));
    }
    if (response->header.kind == protocol::MessageKind::ErrorResponse) {
        auto error = protocol::decode_error_response(response->payload);
        if (!error) {
            co_return std::unexpected(std::move(error.error()));
        }
        co_return std::unexpected(ClientError {
            ClientErrorCode::ServerError,
            error->message,
            ClientErrorContext {.server_code = error->code},
        });
    }
    if (response->header.kind != protocol::MessageKind::ExecuteSqlResponse) {
        co_return std::unexpected(ClientError {
            ClientErrorCode::UnexpectedResponse,
            "expected execute SQL response",
        });
    }

    auto decoded = protocol::decode_execute_sql_response(response->payload);
    if (!decoded) {
        co_return std::unexpected(std::move(decoded.error()));
    }
    auto result = to_core_result(std::move(*decoded));
    if (!result) {
        co_return std::unexpected(std::move(result.error()));
    }
    co_return std::move(*result);
}

asio::awaitable<std::expected<void, ClientError>> Client::close()
{
    if (!socket_.is_open()) {
        co_return std::expected<void, ClientError> {};
    }
    auto written = co_await net::async_write_frame(
        socket_,
        make_frame(protocol::MessageKind::CloseRequest, next_request_id())
    );
    std::error_code ignored;
    socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ignored);
    socket_.close(ignored);
    if (!written) {
        co_return std::unexpected(std::move(written.error()));
    }
    co_return std::expected<void, ClientError> {};
}

std::uint64_t Client::next_request_id() noexcept
{
    return next_request_id_++;
}

asio::awaitable<std::expected<protocol::Frame, ClientError>> Client::roundtrip(protocol::Frame frame)
{
    const auto request_id = frame.header.request_id;
    auto written = co_await net::async_write_frame(socket_, frame);
    if (!written) {
        co_return std::unexpected(std::move(written.error()));
    }

    auto response = co_await net::async_read_frame(socket_);
    if (!response) {
        co_return std::unexpected(std::move(response.error()));
    }
    if (response->header.request_id != request_id) {
        co_return std::unexpected(ClientError {
            ClientErrorCode::UnexpectedResponse,
            "response request id mismatch",
        });
    }
    co_return std::move(*response);
}

} // namespace litedb::client
