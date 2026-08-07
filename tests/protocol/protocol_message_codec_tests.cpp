#include "protocol/message.hpp"

#include "core/io/io_error.hpp"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{

using namespace litedb::protocol;

void require(bool condition, const char * message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

unsigned int hex_digit(char value)
{
    if (value >= '0' && value <= '9') {
        return static_cast<unsigned int>(value - '0');
    }
    if (value >= 'a' && value <= 'f') {
        return static_cast<unsigned int>(value - 'a') + 10U;
    }
    throw std::runtime_error("invalid hexadecimal fixture");
}

std::vector<std::byte> bytes_from_hex(std::string_view hex)
{
    require(hex.size() % 2 == 0, "hexadecimal fixture must contain complete bytes");

    std::vector<std::byte> bytes;
    bytes.reserve(hex.size() / 2);
    for (std::size_t index = 0; index < hex.size(); index += 2) {
        const auto value = (hex_digit(hex[index]) << 4U) | hex_digit(hex[index + 1]);
        bytes.push_back(static_cast<std::byte>(value));
    }
    return bytes;
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

void test_response_golden_bytes()
{
    ExecuteSqlResponse response;
    response.kind = ResultKind::RowSet;
    response.affected_rows = 7;
    response.columns = {
        Column {.name = "n", .type = LogicalType {LogicalTypeId::Null, std::nullopt}},
        Column {.name = "b", .type = LogicalType {LogicalTypeId::Boolean, std::nullopt}},
        Column {.name = "i", .type = LogicalType {LogicalTypeId::Integer, std::nullopt}},
        Column {.name = "l", .type = LogicalType {LogicalTypeId::BigInt, std::nullopt}},
        Column {.name = "f", .type = LogicalType {LogicalTypeId::Float, std::nullopt}},
        Column {.name = "d", .type = LogicalType {LogicalTypeId::Double, std::nullopt}},
        Column {.name = "s", .type = LogicalType {LogicalTypeId::Varchar, 64}},
        Column {.name = "v", .type = LogicalType {LogicalTypeId::Vector, 1}},
    };
    response.rows.push_back(Row {
        .values = {
            Value {.data = std::monostate {}},
            Value {.data = true},
            Value {.data = std::int32_t {2}},
            Value {.data = std::int64_t {3}},
            Value {.data = 4.0F},
            Value {.data = 5.0},
            Value {.data = std::string {"A"}},
            Value {.data = VectorValue {0.5}},
        },
    });

    const auto expected = bytes_from_hex(
        "01"                 // RowSet result kind
        "0000000000000007"   // affected rows
        "00"                 // selected database is absent
        "00000008"           // column count
        "000000016e0000"     // n: Null
        "00000001620100"     // b: Boolean
        "00000001690200"     // i: Integer
        "000000016c0300"     // l: BigInt
        "00000001660400"     // f: Float
        "00000001640500"     // d: Double
        "000000017306010000000000000040" // s: Varchar(64)
        "000000017607010000000000000001" // v: Vector(1)
        "00000001"           // row count
        "00000008"           // values in the row
        "00"                 // null
        "0101"               // true
        "0200000002"         // int32 2
        "030000000000000003" // int64 3
        "0440800000"         // float 4.0
        "054014000000000000" // double 5.0
        "060000000141"       // string "A"
        "07000000013fe0000000000000" // vector {0.5}
    );

    auto encoded = encode_execute_sql_response(response);
    require(encoded.has_value(), "golden SQL response should encode");
    require(*encoded == expected, "SQL response wire format changed");

    auto decoded = decode_execute_sql_response(expected);
    require(decoded.has_value(), "v1 golden SQL response should decode");
    require(decoded->kind == ResultKind::RowSet && decoded->affected_rows == 7,
            "golden response metadata mismatch");
    require(!decoded->selected_database_name.has_value(), "golden response database marker mismatch");
    require(decoded->columns.size() == 8 && decoded->rows.size() == 1,
            "golden response shape mismatch");
    for (std::size_t index = 0; index < decoded->columns.size(); ++index) {
        require(decoded->columns[index].type.id == static_cast<LogicalTypeId>(index),
                "golden response logical type mismatch");
    }
    require(decoded->columns[6].type.parameter == 64,
            "golden response varchar parameter mismatch");
    require(decoded->columns[7].type.parameter == 1,
            "golden response vector parameter mismatch");

    const auto & values = decoded->rows[0].values;
    require(std::holds_alternative<std::monostate>(values[0].data), "golden null value mismatch");
    require(std::get<bool>(values[1].data), "golden boolean value mismatch");
    require(std::get<std::int32_t>(values[2].data) == 2, "golden integer value mismatch");
    require(std::get<std::int64_t>(values[3].data) == 3, "golden bigint value mismatch");
    require(std::get<float>(values[4].data) == 4.0F, "golden float value mismatch");
    require(std::get<double>(values[5].data) == 5.0, "golden double value mismatch");
    require(std::get<std::string>(values[6].data) == "A", "golden string value mismatch");
    const auto & vector = std::get<VectorValue>(values[7].data);
    require(vector.size() == 1 && vector[0] == 0.5, "golden vector value mismatch");
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
        test_response_golden_bytes();
        test_limits_and_passthrough_io_error();
        test_invalid_payload_markers_and_row_width();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
    return 0;
}
