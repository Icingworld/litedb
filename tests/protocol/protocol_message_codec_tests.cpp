#include "protocol/message.hpp"

#include "core/io/io_error.hpp"

#include <cstddef>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

using namespace litedb::protocol;

void require(bool condition, const char * message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void test_handshake_and_sql_request()
{
    auto hello_payload = encode_hello_request(HelloRequest {});
    require(hello_payload.has_value(), "hello request should encode");
    auto hello = decode_hello_request(*hello_payload);
    require(hello.has_value(), "hello request should decode");
    require(hello->min_version == ProtocolVersion && hello->max_version == ProtocolVersion,
            "hello version range mismatch");

    auto sql_payload = encode_execute_sql_request("SELECT 1;");
    require(sql_payload.has_value(), "SQL request should encode");
    auto sql = decode_execute_sql_request(*sql_payload);
    require(sql.has_value() && sql->sql == "SELECT 1;", "SQL request mismatch");
}

void test_response_roundtrip()
{
    ExecuteSqlResponse response;
    response.kind = ResultKind::RowSet;
    response.affected_rows = 7;
    response.selected_database_name = "demo";
    response.columns = {
        Column {.name = "null_value", .type = LogicalType {LogicalTypeId::Null, std::nullopt}},
        Column {.name = "bool_value", .type = LogicalType {LogicalTypeId::Boolean, std::nullopt}},
        Column {.name = "integer_value", .type = LogicalType {LogicalTypeId::Integer, std::nullopt}},
        Column {.name = "bigint_value", .type = LogicalType {LogicalTypeId::BigInt, std::nullopt}},
        Column {.name = "float_value", .type = LogicalType {LogicalTypeId::Float, std::nullopt}},
        Column {.name = "double_value", .type = LogicalType {LogicalTypeId::Double, std::nullopt}},
        Column {.name = "text_value", .type = LogicalType {LogicalTypeId::Varchar, 64}},
        Column {.name = "vector_value", .type = LogicalType {LogicalTypeId::Vector, 3}},
    };
    response.rows.push_back(Row {
        .values = {
            Value {.data = std::monostate {}},
            Value {.data = true},
            Value {.data = std::int32_t {2}},
            Value {.data = std::int64_t {3}},
            Value {.data = 4.0F},
            Value {.data = 5.0},
            Value {.data = std::string {"alice"}},
            Value {.data = VectorValue {0.1, 0.2, 0.3}},
        },
    });

    auto payload = encode_execute_sql_response(response);
    require(payload.has_value(), "SQL response should encode");
    auto decoded = decode_execute_sql_response(*payload);
    require(decoded.has_value(), "SQL response should decode");
    require(decoded->columns.size() == 8 && decoded->rows.size() == 1, "response shape mismatch");
    require(decoded->rows[0].values.size() == decoded->columns.size(), "row width mismatch");
    require(std::get<std::string>(decoded->rows[0].values[6].data) == "alice", "string value mismatch");
    require(std::get<VectorValue>(decoded->rows[0].values[7].data).size() == 3, "vector value mismatch");

    auto error_payload = encode_error_response(ErrorResponse {.code = 0x1203, .message = "boom"});
    require(error_payload.has_value(), "error response should encode");
    auto error = decode_error_response(*error_payload);
    require(error.has_value() && error->code == 0x1203 && error->message == "boom",
            "error response mismatch");
}

void test_limits_and_passthrough_io_error()
{
    auto payload = encode_execute_sql_request("SELECT 1;");
    require(payload.has_value(), "SQL request should encode");
    ProtocolDecodeLimits limits;
    limits.max_sql_bytes = 2;
    auto limited = decode_execute_sql_request(*payload, limits);
    require(!limited && limited.error().is(litedb::core::io::IoErrorCode::ValueTooLarge),
            "IO value limit should pass through unchanged");

    const std::vector<std::byte> truncated {
        static_cast<std::byte>(0), static_cast<std::byte>(0), static_cast<std::byte>(0), static_cast<std::byte>(8),
        static_cast<std::byte>('S'),
    };
    auto decoded = decode_execute_sql_request(truncated);
    require(!decoded && decoded.error().is(litedb::core::io::IoErrorCode::UnexpectedEof),
            "IO EOF should pass through unchanged");
}

void test_invalid_payload_markers_and_row_width()
{
    ExecuteSqlResponse response;
    response.columns = {
        Column {.name = "flag", .type = LogicalType {LogicalTypeId::Boolean, std::nullopt}},
    };
    response.rows.push_back(Row {.values = {Value {.data = true}}});
    auto payload = encode_execute_sql_response(response);
    require(payload.has_value(), "boolean response should encode");

    auto invalid_bool = *payload;
    invalid_bool.back() = static_cast<std::byte>(2);
    auto bool_result = decode_execute_sql_response(invalid_bool);
    require(!bool_result && bool_result.error().is(ProtocolErrorCode::InvalidPayload),
            "invalid boolean marker was accepted");

    auto invalid_optional = *payload;
    invalid_optional[9] = static_cast<std::byte>(2);
    auto optional_result = decode_execute_sql_response(invalid_optional);
    require(!optional_result && optional_result.error().is(ProtocolErrorCode::InvalidPayload),
            "invalid optional marker was accepted");

    auto invalid_width = *payload;
    invalid_width[invalid_width.size() - 3] = static_cast<std::byte>(0);
    auto width_result = decode_execute_sql_response(invalid_width);
    require(!width_result && width_result.error().is(ProtocolErrorCode::InvalidPayload),
            "row width mismatch was accepted");

    ProtocolDecodeLimits limits;
    limits.max_columns = 0;
    auto limited = decode_execute_sql_response(*payload, limits);
    require(!limited && limited.error().is(ProtocolErrorCode::ResourceLimitExceeded),
            "column resource limit was ignored");
}

} // namespace

int main()
{
    try {
        test_handshake_and_sql_request();
        test_response_roundtrip();
        test_limits_and_passthrough_io_error();
        test_invalid_payload_markers_and_row_width();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
    return 0;
}
