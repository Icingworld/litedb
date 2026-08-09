#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "protocol/constants.hpp"
#include "protocol/protocol_error.hpp"

namespace litedb::protocol
{

// 协议解码限制
struct ProtocolDecodeLimits
{
    std::uint32_t max_string_bytes {DefaultMaxStringBytes}; // 最大字符串字节数
    std::uint32_t max_sql_bytes {DefaultMaxSqlBytes}; // 最大 SQL 字节数
    std::uint32_t max_columns {DefaultMaxColumns}; // 最大列数
    std::uint32_t max_rows {DefaultMaxRows}; // 最大行数
    std::uint32_t max_values_per_row {DefaultMaxValuesPerRow}; // 最大每行值数
    std::uint32_t max_vector_elements {DefaultMaxVectorElements}; // 最大向量元素数
};

// 连接请求
struct HelloRequest
{
    std::uint16_t min_version {ProtocolVersion};
    std::uint16_t max_version {ProtocolVersion};
};

// 连接响应
struct HelloResponse
{
    std::uint16_t selected_version {ProtocolVersion};
};

// 执行 SQL 请求
struct ExecuteSqlRequest
{
    std::string sql;
};

// 结果类型
enum class ResultKind : std::uint8_t
{
    Command = 0,
    RowSet = 1,
    UseDatabase = 2,
};

// 逻辑类型 ID
enum class LogicalTypeId : std::uint8_t
{
    Null = 0,
    Boolean = 1,
    Integer = 2,
    BigInt = 3,
    Float = 4,
    Double = 5,
    Varchar = 6,
    Vector = 7,
};

// 逻辑类型
struct LogicalType
{
    LogicalTypeId id {LogicalTypeId::Null};
    std::optional<std::uint64_t> parameter;
};

// 向量值
using VectorValue = std::vector<double>;

// 值数据
using ValueData = std::variant<
    std::monostate,
    bool,
    std::int32_t,
    std::int64_t,
    float,
    double,
    std::string,
    VectorValue>;

// 值
struct Value
{
    ValueData data;
};

// 列
struct Column
{
    std::string name;
    LogicalType type;
};

// 行
struct Row
{
    std::vector<Value> values;
};

// 执行 SQL 响应
struct ExecuteSqlResponse
{
    ResultKind kind {ResultKind::Command};
    std::uint64_t affected_rows {0};
    std::optional<std::string> selected_database_name;
    std::vector<Column> columns;
    std::vector<Row> rows;
};

// 错误响应
struct ErrorResponse
{
    std::uint16_t code {0};
    std::string message;
};

// 编码连接请求
[[nodiscard]]
std::expected<std::vector<std::byte>, ProtocolError> encode_hello_request(
    const HelloRequest & request
);

// 解码连接请求
[[nodiscard]]
std::expected<HelloRequest, ProtocolError>
decode_hello_request(std::span<const std::byte> payload, ProtocolDecodeLimits limits = {});

// 编码连接响应
[[nodiscard]]
std::expected<std::vector<std::byte>, ProtocolError> encode_hello_response(
    const HelloResponse & response
);

// 解码连接响应
[[nodiscard]]
std::expected<HelloResponse, ProtocolError>
decode_hello_response(std::span<const std::byte> payload, ProtocolDecodeLimits limits = {});

// 编码执行 SQL 请求
[[nodiscard]]
std::expected<std::vector<std::byte>, ProtocolError> encode_execute_sql_request(
    std::string_view sql
);

// 解码执行 SQL 请求
[[nodiscard]]
std::expected<ExecuteSqlRequest, ProtocolError>
decode_execute_sql_request(std::span<const std::byte> payload, ProtocolDecodeLimits limits = {});

// 编码执行 SQL 响应
[[nodiscard]]
std::expected<std::vector<std::byte>, ProtocolError> encode_execute_sql_response(
    const ExecuteSqlResponse & result
);

// 解码执行 SQL 响应
[[nodiscard]]
std::expected<ExecuteSqlResponse, ProtocolError>
decode_execute_sql_response(std::span<const std::byte> payload, ProtocolDecodeLimits limits = {});

// 编码错误响应
[[nodiscard]]
std::expected<std::vector<std::byte>, ProtocolError> encode_error_response(
    const ErrorResponse & response
);

// 解码错误响应
[[nodiscard]]
std::expected<ErrorResponse, ProtocolError>
decode_error_response(std::span<const std::byte> payload, ProtocolDecodeLimits limits = {});

} // namespace litedb::protocol
