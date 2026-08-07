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

/**
 * @brief 协议解码限制
 */
struct ProtocolDecodeLimits
{
    std::uint32_t max_string_bytes {DefaultMaxStringBytes};             ///< 最大字符串字节数
    std::uint32_t max_sql_bytes {DefaultMaxSqlBytes};                   ///< 最大 SQL 字节数
    std::uint32_t max_columns {DefaultMaxColumns};                      ///< 最大列数
    std::uint32_t max_rows {DefaultMaxRows};                            ///< 最大行数
    std::uint32_t max_values_per_row {DefaultMaxValuesPerRow};          ///< 最大每行值数
    std::uint32_t max_vector_elements {DefaultMaxVectorElements};       ///< 最大向量元素数
};

/**
 * @brief 连接请求
 */
struct HelloRequest
{
    std::uint16_t min_version {ProtocolVersion};                        ///< 最小协议版本
    std::uint16_t max_version {ProtocolVersion};                        ///< 最大协议版本
};

/**
 * @brief 连接响应
 */
struct HelloResponse
{
    std::uint16_t selected_version {ProtocolVersion};                   ///< 选定的协议版本
};

/**
 * @brief 执行 SQL 请求
 */
struct ExecuteSqlRequest
{
    std::string sql;                                                    ///< SQL 语句
};

/**
 * @brief 结果类型
 */
enum class ResultKind : std::uint8_t
{
    Command = 0,                                                        ///< 命令
    RowSet = 1,                                                         ///< 行集
    UseDatabase = 2,                                                    ///< 切换数据库
};

/**
 * @brief 逻辑类型 ID
 */
enum class LogicalTypeId : std::uint8_t
{
    Null = 0,                                                           ///< 空
    Boolean = 1,                                                        ///< 布尔
    Integer = 2,                                                        ///< 整数
    BigInt = 3,                                                         ///< 大整数
    Float = 4,                                                          ///< 浮点数
    Double = 5,                                                         ///< 双精度浮点数
    Varchar = 6,                                                        ///< 字符串
    Vector = 7,                                                         ///< 向量
};

/**
 * @brief 逻辑类型
 */
struct LogicalType
{
    LogicalTypeId id {LogicalTypeId::Null};                            ///< 类型 ID
    std::optional<std::uint64_t> parameter;                            ///< 参数
};

/**
 * @brief 向量值
 */
using VectorValue = std::vector<double>;

/**
 * @brief 值数据
 */
using ValueData = std::variant<
    std::monostate,
    bool,
    std::int32_t,
    std::int64_t,
    float,
    double,
    std::string,
    VectorValue
>;

/**
 * @brief 值
 */
struct Value
{
    ValueData data;                                                ///< 值数据
};

/**
 * @brief 列
 */
struct Column
{
    std::string name;                                                ///< 列名
    LogicalType type;                                                ///< 列类型
};

/**
 * @brief 行
 */
struct Row
{
    std::vector<Value> values;                                        ///< 值列表
};

/**
 * @brief 执行 SQL 响应
 */
struct ExecuteSqlResponse
{
    ResultKind kind {ResultKind::Command};                            ///< 结果类型
    std::uint64_t affected_rows {0};                                  ///< 受影响的行数
    std::optional<std::string> selected_database_name;                ///< 选定的数据库名称
    std::vector<Column> columns;                                      ///< 列列表
    std::vector<Row> rows;                                            ///< 行列表
};

/**
 * @brief 错误响应
 */
struct ErrorResponse
{
    std::uint16_t code {0};                                             ///< 错误码
    std::string message;                                                ///< 错误消息
};

/**
 * @brief 编码连接请求
 * @param request 连接请求
 * @return 编码后的字节序列
 */
[[nodiscard]]
std::expected<std::vector<std::byte>, ProtocolError>
encode_hello_request(
    const HelloRequest & request
);

/**
 * @brief 解码连接请求
 * @param payload 字节序列
 * @param limits 解码限制
 * @return 解码后的连接请求
 */
[[nodiscard]]
std::expected<HelloRequest, ProtocolError>
decode_hello_request(
    std::span<const std::byte> payload,
    ProtocolDecodeLimits limits = {}
);

/**
 * @brief 编码连接响应
 * @param response 连接响应
 * @return 编码后的字节序列
 */
[[nodiscard]]
std::expected<std::vector<std::byte>, ProtocolError>
encode_hello_response(
    const HelloResponse & response
);

/**
 * @brief 解码连接响应
 * @param payload 字节序列
 * @param limits 解码限制
 * @return 解码后的连接响应
 */
[[nodiscard]]
std::expected<HelloResponse, ProtocolError>
decode_hello_response(
    std::span<const std::byte> payload,
    ProtocolDecodeLimits limits = {}
);

/**
 * @brief 编码执行 SQL 请求
 * @param sql SQL 语句
 * @return 编码后的字节序列
 */
[[nodiscard]]
std::expected<std::vector<std::byte>, ProtocolError>
encode_execute_sql_request(
    std::string_view sql
);

/**
 * @brief 解码执行 SQL 请求
 * @param payload 字节序列
 * @param limits 解码限制
 * @return 解码后的执行 SQL 请求
 */
[[nodiscard]]
std::expected<ExecuteSqlRequest, ProtocolError>
decode_execute_sql_request(
    std::span<const std::byte> payload,
    ProtocolDecodeLimits limits = {}
);

/**
 * @brief 编码执行 SQL 响应
 * @param result 执行 SQL 响应
 * @return 编码后的字节序列
 */
[[nodiscard]]
std::expected<std::vector<std::byte>, ProtocolError>
encode_execute_sql_response(
    const ExecuteSqlResponse & result
);

/**
 * @brief 解码执行 SQL 响应
 * @param payload 字节序列
 * @param limits 解码限制
 * @return 解码后的执行 SQL 响应
 */
[[nodiscard]]
std::expected<ExecuteSqlResponse, ProtocolError>
decode_execute_sql_response(
    std::span<const std::byte> payload,
    ProtocolDecodeLimits limits = {}
);

/**
 * @brief 编码错误响应
 * @param response 错误响应
 * @return 编码后的字节序列
 */
[[nodiscard]]
std::expected<std::vector<std::byte>, ProtocolError>
encode_error_response(
    const ErrorResponse & response
);

/**
 * @brief 解码错误响应
 * @param payload 字节序列
 * @param limits 解码限制
 * @return 解码后的错误响应
 */
[[nodiscard]]
std::expected<ErrorResponse, ProtocolError>
decode_error_response(
    std::span<const std::byte> payload,
    ProtocolDecodeLimits limits = {}
);

} // namespace litedb::protocol
