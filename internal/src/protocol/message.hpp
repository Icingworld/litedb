#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

#include "core/error/error.hpp"
#include "core/executor/execution_result.hpp"

namespace litedb::protocol
{

inline constexpr std::uint16_t ProtocolVersion = 1;
inline constexpr std::size_t FrameHeaderSize = 16;
inline constexpr std::size_t DefaultMaxFrameSize = 16 * 1024 * 1024;

enum class MessageKind : std::uint16_t
{
    ExecuteSqlRequest = 1,
    ExecuteSqlResponse = 2,
    ErrorResponse = 3,
    PingRequest = 4,
    PongResponse = 5,
};

enum class ProtocolErrorCode : std::uint8_t
{
    InvalidFrame,
    InvalidVersion,
    InvalidMessageKind,
    UnexpectedEnd,
    InvalidPayload,
    FrameTooLarge,
};

using ProtocolError = core::error::Error;

struct FrameHeader
{
    std::uint32_t payload_size {0};
    std::uint16_t version {ProtocolVersion};
    MessageKind kind {MessageKind::ErrorResponse};
    std::uint64_t request_id {0};
};

struct Frame
{
    FrameHeader header;
    std::vector<std::uint8_t> payload;
};

struct ExecuteSqlRequest
{
    std::string sql;
};

struct ErrorResponse
{
    std::uint16_t code {0};
    std::string message;
};

[[nodiscard]]
std::vector<std::uint8_t> encode_frame(const Frame & frame);

[[nodiscard]]
std::expected<FrameHeader, ProtocolError> decode_frame_header(const std::uint8_t * data, std::size_t size);

[[nodiscard]]
std::expected<Frame, ProtocolError> decode_frame(const std::uint8_t * data, std::size_t size);

[[nodiscard]]
std::vector<std::uint8_t> encode_execute_sql_request(std::string_view sql);

[[nodiscard]]
std::expected<ExecuteSqlRequest, ProtocolError> decode_execute_sql_request(const std::vector<std::uint8_t> & payload);

[[nodiscard]]
std::vector<std::uint8_t> encode_execute_sql_response(const core::executor::ExecutionResult & result);

[[nodiscard]]
std::expected<core::executor::ExecutionResult, ProtocolError> decode_execute_sql_response(
    const std::vector<std::uint8_t> & payload
);

[[nodiscard]]
std::vector<std::uint8_t> encode_error_response(const ErrorResponse & response);

[[nodiscard]]
std::expected<ErrorResponse, ProtocolError> decode_error_response(const std::vector<std::uint8_t> & payload);

} // namespace litedb::protocol

namespace litedb::core::error
{
template <>
struct ErrorTraits<::litedb::protocol::ProtocolErrorCode>
{
    static constexpr ErrorCategory category = ErrorCategory::Protocol;
};
} // namespace litedb::core::error
