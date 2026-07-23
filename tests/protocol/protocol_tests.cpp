#include "protocol/message.hpp"

#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

using namespace litedb::core::common;
using namespace litedb::core::executor;
using namespace litedb::protocol;

void require(bool condition, const char * message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <typename T>
const T & get_value(const Value & value)
{
    return std::get<T>(value.data());
}

LogicalType type(LogicalTypeId id, std::optional<std::size_t> parameter = std::nullopt)
{
    return LogicalType {id, parameter};
}

void test_frame_roundtrip()
{
    Frame frame {
        .header = FrameHeader {
            .payload_size = 0,
            .version = ProtocolVersion,
            .kind = MessageKind::PingRequest,
            .request_id = 42,
        },
        .payload = {1, 2, 3},
    };

    auto encoded = encode_frame(frame);
    auto decoded = decode_frame(encoded.data(), encoded.size());
    require(decoded.has_value(), "frame should decode");
    require(decoded->header.payload_size == 3, "payload size mismatch");
    require(decoded->header.kind == MessageKind::PingRequest, "message kind mismatch");
    require(decoded->header.request_id == 42, "request id mismatch");
    require(decoded->payload == frame.payload, "payload mismatch");
}

void test_execute_sql_request_roundtrip()
{
    auto payload = encode_execute_sql_request("SELECT 1;");
    auto decoded = decode_execute_sql_request(payload);
    require(decoded.has_value(), "request should decode");
    require(decoded->sql == "SELECT 1;", "sql mismatch");
}

void test_execute_sql_response_roundtrip()
{
    ExecutionResult result;
    result.kind = ExecutionResultKind::RowSet;
    result.affected_rows = 7;
    result.columns.push_back(ExecutionColumn {.name = "id", .type = type(LogicalTypeId::BigInt)});
    result.columns.push_back(ExecutionColumn {.name = "name", .type = type(LogicalTypeId::Varchar, 64)});
    result.columns.push_back(ExecutionColumn {.name = "embedding", .type = type(LogicalTypeId::Vector, 3)});
    result.rows.push_back(ExecutionRow {
        .values = {
            Value {std::int64_t {1}},
            Value {std::string {"alice"}},
            Value {VectorValue {0.1, 0.2, 0.3}},
        },
    });

    auto payload = encode_execute_sql_response(result);
    auto decoded = decode_execute_sql_response(payload);
    require(decoded.has_value(), "response should decode");
    require(decoded->kind == ExecutionResultKind::RowSet, "result kind mismatch");
    require(decoded->affected_rows == 7, "affected rows mismatch");
    require(decoded->columns.size() == 3, "column count mismatch");
    require(decoded->columns[1].type.parameter == 64, "column parameter mismatch");
    require(decoded->rows.size() == 1, "row count mismatch");
    require(get_value<std::int64_t>(decoded->rows[0].values[0]) == 1, "id value mismatch");
    require(get_value<std::string>(decoded->rows[0].values[1]) == "alice", "name value mismatch");
    require(get_value<VectorValue>(decoded->rows[0].values[2]).size() == 3, "vector value mismatch");
}

void test_error_response_roundtrip()
{
    auto payload = encode_error_response(ErrorResponse {.code = 9, .message = "boom"});
    auto decoded = decode_error_response(payload);
    require(decoded.has_value(), "error should decode");
    require(decoded->code == 9, "error code mismatch");
    require(decoded->message == "boom", "error message mismatch");
}

void test_truncated_payload_fails()
{
    std::vector<std::uint8_t> payload {0, 0, 0, 8, 'S'};
    auto decoded = decode_execute_sql_request(payload);
    require(!decoded.has_value(), "truncated payload should fail");
    require(decoded.error().code == ProtocolErrorCode::UnexpectedEnd, "truncated error code mismatch");
}

} // namespace

int main()
{
    try {
        test_frame_roundtrip();
        test_execute_sql_request_roundtrip();
        test_execute_sql_response_roundtrip();
        test_error_response_roundtrip();
        test_truncated_payload_fails();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
