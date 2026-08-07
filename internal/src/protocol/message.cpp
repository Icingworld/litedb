#include "protocol/message.hpp"

#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "core/io/binary_io.hpp"
#include "core/io/buffer_byte_reader.hpp"
#include "core/io/buffer_byte_writer.hpp"

namespace litedb::protocol
{

namespace
{

using Reader = core::io::BigEndianBinaryReader;
using Writer = core::io::BigEndianBinaryWriter;

ProtocolError make_error(ProtocolErrorCode code, std::string_view message)
{
    return ProtocolError {code, message};
}

/**
 * @brief 确保 payload 大小不超过最大限制
 * @param payload 要检查的 payload
 * @return 如果 payload 大小不超过最大限制，则返回空结果；否则返回错误
 */
std::expected<void, ProtocolError> ensure_payload_size(
    std::span<const std::byte> payload
)
{
    if (payload.size() > MaxPayloadSize) {
        return std::unexpected(make_error(
            ProtocolErrorCode::FrameTooLarge,
            "message payload exceeds the configured maximum"
        ));
    }
    return {};
}

/**
 * @brief 创建读取器
 * @param source 源
 * @param payload 字节序列
 * @param max_string 最大字符串字节数
 * @return 创建的读取器
 */
Reader make_reader(
    core::io::BufferByteReader & source,
    std::span<const std::byte> payload,
    std::uint32_t max_string
)
{
    return Reader {
        source,
        core::io::BinaryDecodeLimits {
            .max_total_bytes = payload.size(),
            .max_string_bytes = max_string,
        },
    };
}

/**
 * @brief 确保读取器已经读取完所有数据
 * @param reader 要检查的读取器
 * @param name 要检查的名称
 * @return 如果读取器已经读取完所有数据，则返回空结果；否则返回错误
 */
std::expected<void, ProtocolError> ensure_done(
    const Reader & reader,
    std::string_view name
)
{
    if (reader.remaining_bytes() != 0) {
        std::string message {name};
        message += " has trailing bytes";
        return std::unexpected(make_error(
            ProtocolErrorCode::InvalidPayload,
            message
        ));
    }
    return {};
}

/**
 * @brief 写入计数
 * @param writer 写入器
 * @param count 计数
 * @param name 名称
 * @return 如果写入成功，则返回空结果；否则返回错误
 */
std::expected<void, ProtocolError> write_count(
    Writer & writer,
    std::size_t count,
    std::string_view name
)
{
    // 确保计数不超过最大限制
    if (count > std::numeric_limits<std::uint32_t>::max()) {
        std::string message {name};
        message += " exceeds the wire count limit";
        return std::unexpected(make_error(
            ProtocolErrorCode::ResourceLimitExceeded,
            message
        ));
    }
    return writer.write_u32(static_cast<std::uint32_t>(count));
}

/**
 * @brief 读取逻辑类型
 * @param reader 读取器
 * @return 读取的逻辑类型
 */
std::expected<LogicalType, ProtocolError> read_logical_type(Reader & reader)
{
    auto id = reader.read_u8();
    if (!id) {
        return std::unexpected(std::move(id.error()));
    }
    if (*id > static_cast<std::uint8_t>(LogicalTypeId::Vector)) {
        return std::unexpected(make_error(
            ProtocolErrorCode::InvalidPayload,
            "invalid logical type id"
        ));
    }

    auto has_parameter = reader.read_u8();
    if (!has_parameter) {
        return std::unexpected(std::move(has_parameter.error()));
    }
    if (*has_parameter > 1) {
        return std::unexpected(make_error(
            ProtocolErrorCode::InvalidPayload,
            "logical type parameter marker must be zero or one"
        ));
    }

    std::optional<std::uint64_t> parameter;
    if (*has_parameter != 0) {
        auto value = reader.read_u64();
        if (!value) {
            return std::unexpected(std::move(value.error()));
        }
        parameter = *value;
    }

    return LogicalType {
        .id = static_cast<LogicalTypeId>(*id),
        .parameter = parameter,
    };
}

/**
 * @brief 写入逻辑类型
 * @param writer 写入器
 * @param type 逻辑类型
 * @return 如果写入成功，则返回空结果；否则返回错误
 */
std::expected<void, ProtocolError> write_logical_type(
    Writer & writer,
    const LogicalType & type
)
{
    if (static_cast<std::uint8_t>(type.id) > static_cast<std::uint8_t>(LogicalTypeId::Vector)) {
        return std::unexpected(make_error(
            ProtocolErrorCode::InvalidPayload,
            "invalid logical type id"
        ));
    }
    if (auto result = writer.write_u8(static_cast<std::uint8_t>(type.id)); !result) {
        return std::unexpected(std::move(result.error()));
    }
    if (auto result = writer.write_u8(type.parameter.has_value() ? 1U : 0U); !result) {
        return std::unexpected(std::move(result.error()));
    }
    if (type.parameter.has_value()) {
        return writer.write_u64(*type.parameter);
    }
    return {};
}

/**
 * @brief 读取值
 * @param reader 读取器
 * @param limits 解码限制
 * @return 读取的值
 */
std::expected<Value, ProtocolError> read_value(
    Reader & reader,
    const ProtocolDecodeLimits & limits
)
{
    auto tag = reader.read_u8();
    if (!tag) {
        return std::unexpected(std::move(tag.error()));
    }

    switch (static_cast<LogicalTypeId>(*tag)) {
    case LogicalTypeId::Null:
        return Value {.data = std::monostate {}};
    case LogicalTypeId::Boolean: {
        auto value = reader.read_u8();
        if (!value) {
            return std::unexpected(std::move(value.error()));
        }
        if (*value > 1) {
            return std::unexpected(make_error(
                ProtocolErrorCode::InvalidPayload,
                "boolean value must be zero or one"
            ));
        }
        return Value {.data = (*value != 0)};
    }
    case LogicalTypeId::Integer: {
        auto value = reader.read_i32();
        if (!value) {
            return std::unexpected(std::move(value.error()));
        }
        return Value {.data = *value};
    }
    case LogicalTypeId::BigInt: {
        auto value = reader.read_i64();
        if (!value) {
            return std::unexpected(std::move(value.error()));
        }
        return Value {.data = *value};
    }
    case LogicalTypeId::Float: {
        auto value = reader.read_f32();
        if (!value) {
            return std::unexpected(std::move(value.error()));
        }
        return Value {.data = *value};
    }
    case LogicalTypeId::Double: {
        auto value = reader.read_f64();
        if (!value) {
            return std::unexpected(std::move(value.error()));
        }
        return Value {.data = *value};
    }
    case LogicalTypeId::Varchar: {
        auto value = reader.read_string();
        if (!value) {
            return std::unexpected(std::move(value.error()));
        }
        return Value {.data = std::move(*value)};
    }
    case LogicalTypeId::Vector: {
        auto count = reader.read_u32();
        if (!count) {
            return std::unexpected(std::move(count.error()));
        }
        if (*count > limits.max_vector_elements) {
            return std::unexpected(make_error(
                ProtocolErrorCode::ResourceLimitExceeded,
                "vector exceeds the configured element limit"
            ));
        }
        if (*count > reader.remaining_bytes() / sizeof(double)) {
            return std::unexpected(make_error(
                ProtocolErrorCode::UnexpectedEnd,
                "vector length exceeds the remaining payload"
            ));
        }
        VectorValue values;
        values.reserve(*count);
        for (std::uint32_t index = 0; index < *count; ++index) {
            auto value = reader.read_f64();
            if (!value) {
                return std::unexpected(std::move(value.error()));
            }
            values.push_back(*value);
        }
        return Value {.data = std::move(values)};
    }
    }

    return std::unexpected(make_error(
        ProtocolErrorCode::InvalidPayload,
        "invalid value tag"
    ));
}

/**
 * @brief 写入值
 * @param writer 写入器
 * @param value 值
 * @return 如果写入成功，则返回空结果；否则返回错误
 */
std::expected<void, ProtocolError> write_value(
    Writer & writer,
    const Value & value
)
{
    return std::visit(
        [&writer](const auto & data) -> std::expected<void, ProtocolError> {
            using T = std::decay_t<decltype(data)>;
            LogicalTypeId kind = LogicalTypeId::Null;
            if constexpr (std::is_same_v<T, std::monostate>) {
                kind = LogicalTypeId::Null;
            } else if constexpr (std::is_same_v<T, bool>) {
                kind = LogicalTypeId::Boolean;
            } else if constexpr (std::is_same_v<T, std::int32_t>) {
                kind = LogicalTypeId::Integer;
            } else if constexpr (std::is_same_v<T, std::int64_t>) {
                kind = LogicalTypeId::BigInt;
            } else if constexpr (std::is_same_v<T, float>) {
                kind = LogicalTypeId::Float;
            } else if constexpr (std::is_same_v<T, double>) {
                kind = LogicalTypeId::Double;
            } else if constexpr (std::is_same_v<T, std::string>) {
                kind = LogicalTypeId::Varchar;
                if (data.size() > DefaultMaxStringBytes) {
                    return std::unexpected(make_error(
                        ProtocolErrorCode::ResourceLimitExceeded,
                        "string value exceeds the configured limit"
                    ));
                }
            } else {
                static_assert(std::is_same_v<T, VectorValue>);
                kind = LogicalTypeId::Vector;
                if (data.size() > std::numeric_limits<std::uint32_t>::max()
                    || data.size() > MaxPayloadSize / sizeof(double)) {
                    return std::unexpected(make_error(
                        ProtocolErrorCode::ResourceLimitExceeded,
                        "vector is too large to encode"
                    ));
                }
            }

            if (auto result = writer.write_u8(static_cast<std::uint8_t>(kind)); !result) {
                return std::unexpected(std::move(result.error()));
            }
            if constexpr (std::is_same_v<T, std::monostate>) {
                return {};
            } else if constexpr (std::is_same_v<T, bool>) {
                return writer.write_u8(data ? 1U : 0U);
            } else if constexpr (std::is_same_v<T, std::int32_t>) {
                return writer.write_i32(data);
            } else if constexpr (std::is_same_v<T, std::int64_t>) {
                return writer.write_i64(data);
            } else if constexpr (std::is_same_v<T, float>) {
                return writer.write_f32(data);
            } else if constexpr (std::is_same_v<T, double>) {
                return writer.write_f64(data);
            } else if constexpr (std::is_same_v<T, std::string>) {
                return writer.write_string(data);
            } else {
                if (auto result = writer.write_u32(static_cast<std::uint32_t>(data.size())); !result) {
                    return std::unexpected(std::move(result.error()));
                }
                for (const auto element : data) {
                    if (auto result = writer.write_f64(element); !result) {
                        return std::unexpected(std::move(result.error()));
                    }
                }
                return {};
            }
        },
        value.data
    );
}

} // namespace

std::expected<std::vector<std::byte>, ProtocolError> encode_hello_request(
    const HelloRequest & request
)
{
    // 验证版本范围
    if (request.min_version > request.max_version) {
        return std::unexpected(make_error(
            ProtocolErrorCode::InvalidPayload,
            "hello version range is invalid"
        ));
    }

    core::io::BufferByteWriter buffer {MaxPayloadSize};
    Writer writer {buffer};

    // 在 payload 中写入版本范围
    if (auto result = writer.write_u16(request.min_version); !result) {
        return std::unexpected(std::move(result.error()));
    }
    if (auto result = writer.write_u16(request.max_version); !result) {
        return std::unexpected(std::move(result.error()));
    }

    return buffer.take_bytes();
}

std::expected<HelloRequest, ProtocolError> decode_hello_request(
    std::span<const std::byte> payload,
    ProtocolDecodeLimits /* limits */
)
{
    // 确保 payload 大小不超过最大限制
    if (auto size = ensure_payload_size(payload); !size) {
        return std::unexpected(std::move(size.error()));
    }

    core::io::BufferByteReader source {payload};
    auto reader = make_reader(source, payload, 0);

    // 读取版本范围
    auto min_version = reader.read_u16();
    if (!min_version) {
        return std::unexpected(std::move(min_version.error()));
    }
    auto max_version = reader.read_u16();
    if (!max_version) {
        return std::unexpected(std::move(max_version.error()));
    }
    if (*min_version > *max_version) {
        return std::unexpected(make_error(
            ProtocolErrorCode::InvalidPayload,
            "hello version range is invalid"
        ));
    }

    // 确保读取器已经读取完所有数据
    if (auto done = ensure_done(reader, "hello request"); !done) {
        return std::unexpected(std::move(done.error()));
    }

    return HelloRequest {
        .min_version = *min_version,
        .max_version = *max_version,
    };
}

std::expected<std::vector<std::byte>, ProtocolError> encode_hello_response(
    const HelloResponse & response
)
{
    // 确保选定的版本是支持的版本
    if (response.selected_version != ProtocolVersion) {
        return std::unexpected(make_error(
            ProtocolErrorCode::UnsupportedVersion,
            "unsupported selected protocol version"
        ));
    }

    core::io::BufferByteWriter buffer {MaxPayloadSize};
    Writer writer {buffer};

    // 在 payload 中写入选定的版本
    if (auto result = writer.write_u16(response.selected_version); !result) {
        return std::unexpected(std::move(result.error()));
    }

    return buffer.take_bytes();
}

std::expected<HelloResponse, ProtocolError> decode_hello_response(
    std::span<const std::byte> payload,
    ProtocolDecodeLimits /* limits */
)
{
    // 确保 payload 大小不超过最大限制
    if (auto size = ensure_payload_size(payload); !size) {
        return std::unexpected(std::move(size.error()));
    }

    core::io::BufferByteReader source {payload};
    auto reader = make_reader(source, payload, 0);

    // 读取选定的版本
    auto version = reader.read_u16();
    if (!version) {
        return std::unexpected(std::move(version.error()));
    }
    if (*version != ProtocolVersion) {
        return std::unexpected(make_error(ProtocolErrorCode::UnsupportedVersion, "unsupported selected protocol version"));
    }

    // 确保读取器已经读取完所有数据
    if (auto done = ensure_done(reader, "hello response"); !done) {
        return std::unexpected(std::move(done.error()));
    }

    return HelloResponse {
        .selected_version = *version
    };
}

std::expected<std::vector<std::byte>, ProtocolError> encode_execute_sql_request(
    std::string_view sql
)
{
    // 确保 SQL 语句不超过最大长度限制
    if (sql.size() > DefaultMaxSqlBytes) {
        return std::unexpected(make_error(
            ProtocolErrorCode::ResourceLimitExceeded,
            "SQL exceeds the configured limit"
        ));
    }

    core::io::BufferByteWriter buffer {MaxPayloadSize};
    Writer writer {buffer};

    // 在 payload 中写入 SQL 语句
    if (auto result = writer.write_string(sql); !result) {
        return std::unexpected(std::move(result.error()));
    }

    return buffer.take_bytes();
}

std::expected<ExecuteSqlRequest, ProtocolError> decode_execute_sql_request(
    std::span<const std::byte> payload,
    ProtocolDecodeLimits limits
)
{
    // 确保 payload 大小不超过最大限制
    if (auto size = ensure_payload_size(payload); !size) {
        return std::unexpected(std::move(size.error()));
    }

    core::io::BufferByteReader source {payload};
    auto reader = make_reader(source, payload, limits.max_sql_bytes);

    // 读取 SQL 语句
    auto sql = reader.read_string();
    if (!sql) {
        return std::unexpected(std::move(sql.error()));
    }

    // 确保读取器已经读取完所有数据
    if (auto done = ensure_done(reader, "execute SQL request"); !done) {
        return std::unexpected(std::move(done.error()));
    }

    return ExecuteSqlRequest {
        .sql = std::move(*sql),
    };
}

std::expected<std::vector<std::byte>, ProtocolError> encode_execute_sql_response(
    const ExecuteSqlResponse & result
)
{
    // 确保结果类型有效、列数和行数不超过最大限制
    if (static_cast<std::uint8_t>(result.kind) > static_cast<std::uint8_t>(ResultKind::UseDatabase)
        || result.columns.size() > DefaultMaxColumns
        || result.rows.size() > DefaultMaxRows) {
        return std::unexpected(make_error(
            ProtocolErrorCode::ResourceLimitExceeded,
            "execution result exceeds the configured limit"
        ));
    }

    core::io::BufferByteWriter buffer {MaxPayloadSize};
    Writer writer {buffer};

    // 在 payload 中写入结果类型
    if (auto value = writer.write_u8(
        static_cast<std::uint8_t>(result.kind)
    ); !value) {
        return std::unexpected(std::move(value.error()));
    }
    // 在 payload 中写入受影响的行数
    if (auto value = writer.write_u64(result.affected_rows); !value) {
        return std::unexpected(std::move(value.error()));
    }
    // 在 payload 中写入选中的数据库名称
    // 如果选中的数据库名称不为空，则写入 1，否则写入 0
    if (auto value = writer.write_u8(
        result.selected_database_name.has_value() ? 1U : 0U
    ); !value) {
        return std::unexpected(std::move(value.error()));
    }
    if (result.selected_database_name.has_value()) {
        if (result.selected_database_name->size() > DefaultMaxStringBytes) {
            return std::unexpected(make_error(
                ProtocolErrorCode::ResourceLimitExceeded,
                "database name exceeds the configured limit"
            ));
        }
        if (auto value = writer.write_string(
            *result.selected_database_name
        ); !value) {
            return std::unexpected(std::move(value.error()));
        }
    }
    // 在 payload 中写入列数
    if (auto value = write_count(writer, result.columns.size(), "column count"); !value) {
        return std::unexpected(std::move(value.error()));
    }
    // 在 payload 中写入列
    for (const auto & column : result.columns) {
        if (column.name.size() > DefaultMaxStringBytes) {
            return std::unexpected(make_error(
                ProtocolErrorCode::ResourceLimitExceeded,
                "column name exceeds the configured limit"
            ));
        }
        // 在 payload 中写入列名
        if (auto value = writer.write_string(column.name); !value) {
            return std::unexpected(std::move(value.error()));
        }
        // 在 payload 中写入列类型
        if (auto value = write_logical_type(writer, column.type); !value) {
            return std::unexpected(std::move(value.error()));
        }
    }
    // 在 payload 中写入行数
    if (auto value = write_count(writer, result.rows.size(), "row count"); !value) {
        return std::unexpected(std::move(value.error()));
    }
    // 在 payload 中写入行
    for (const auto & row : result.rows) {
        if (row.values.size() != result.columns.size() || row.values.size() > DefaultMaxValuesPerRow) {
            return std::unexpected(make_error(
                ProtocolErrorCode::InvalidPayload,
                "row value count does not match columns"
            ));
        }
        // 在 payload 中写入行数
        if (auto value = write_count(writer, row.values.size(), "row value count"); !value) {
            return std::unexpected(std::move(value.error()));
        }
        // 在 payload 中写入行值
        for (const auto & item : row.values) {
            if (auto value = write_value(writer, item); !value) {
                return std::unexpected(std::move(value.error()));
            }
        }
    }

    return buffer.take_bytes();
}

std::expected<ExecuteSqlResponse, ProtocolError> decode_execute_sql_response(
    std::span<const std::byte> payload,
    ProtocolDecodeLimits limits
)
{
    // 确保 payload 大小不超过最大限制
    if (auto size = ensure_payload_size(payload); !size) {
        return std::unexpected(std::move(size.error()));
    }

    core::io::BufferByteReader source {payload};
    auto reader = make_reader(source, payload, limits.max_string_bytes);
    
    // 读取结果类型
    auto kind = reader.read_u8();
    if (!kind) {
        return std::unexpected(std::move(kind.error()));
    }
    if (*kind > static_cast<std::uint8_t>(ResultKind::UseDatabase)) {
        return std::unexpected(make_error(
            ProtocolErrorCode::InvalidPayload,
            "invalid result kind"
        ));
    }

    // 读取受影响的行数
    auto affected_rows = reader.read_u64();
    if (!affected_rows) {
        return std::unexpected(std::move(affected_rows.error()));
    }
    // 读取选中的数据库
    // 如果读取的值为 1，则表示选中了数据库，否则表示未选中数据库
    auto selected_marker = reader.read_u8();
    if (!selected_marker) {
        return std::unexpected(std::move(selected_marker.error()));
    }
    if (*selected_marker > 1) {
        return std::unexpected(make_error(
            ProtocolErrorCode::InvalidPayload,
            "selected database marker must be zero or one"
        ));
    }

    ExecuteSqlResponse result;
    result.kind = static_cast<ResultKind>(*kind);
    result.affected_rows = *affected_rows;
    if (*selected_marker != 0) {
        auto name = reader.read_string();
        if (!name) {
            return std::unexpected(std::move(name.error()));
        }
        result.selected_database_name = std::move(*name);
    }

    // 读取列数
    auto column_count = reader.read_u32();
    if (!column_count) {
        return std::unexpected(std::move(column_count.error()));
    }
    if (*column_count > limits.max_columns) {
        return std::unexpected(make_error(
            ProtocolErrorCode::ResourceLimitExceeded,
            "column count exceeds the configured limit"
        ));
    }
    if (*column_count > reader.remaining_bytes() / 6U) {
        return std::unexpected(make_error(
            ProtocolErrorCode::UnexpectedEnd,
            "column count exceeds the remaining payload"
        ));
    }
    // 读取列
    result.columns.reserve(*column_count);
    for (std::uint32_t index = 0; index < *column_count; ++index) {
        auto name = reader.read_string();
        if (!name) {
            return std::unexpected(std::move(name.error()));
        }
        auto type = read_logical_type(reader);
        if (!type) {
            return std::unexpected(std::move(type.error()));
        }
        result.columns.push_back(Column {.name = std::move(*name), .type = *type});
    }

    // 读取行数
    auto row_count = reader.read_u32();
    if (!row_count) {
        return std::unexpected(std::move(row_count.error()));
    }
    if (*row_count > limits.max_rows) {
        return std::unexpected(make_error(
            ProtocolErrorCode::ResourceLimitExceeded,
            "row count exceeds the configured limit"
        ));
    }
    if (*row_count > reader.remaining_bytes() / 4U) {
        return std::unexpected(make_error(
            ProtocolErrorCode::UnexpectedEnd,
            "row count exceeds the remaining payload"
        ));
    }
    // 读取行
    result.rows.reserve(*row_count);
    for (std::uint32_t row_index = 0; row_index < *row_count; ++row_index) {
        // 读取行值数
        auto value_count = reader.read_u32();
        if (!value_count) {
            return std::unexpected(std::move(value_count.error()));
        }
        if (*value_count != result.columns.size() || *value_count > limits.max_values_per_row) {
            return std::unexpected(make_error(
                ProtocolErrorCode::InvalidPayload,
                "row value count does not match columns"
            ));
        }
        // 读取行值
        Row row;
        row.values.reserve(*value_count);
        for (std::uint32_t value_index = 0; value_index < *value_count; ++value_index) {
            auto value = read_value(reader, limits);
            if (!value) {
                return std::unexpected(std::move(value.error()));
            }
            row.values.push_back(std::move(*value));
        }
        result.rows.push_back(std::move(row));
    }

    // 确保读取器已经读取完所有数据
    if (auto done = ensure_done(reader, "execute SQL response"); !done) {
        return std::unexpected(std::move(done.error()));
    }

    return result;
}

std::expected<std::vector<std::byte>, ProtocolError> encode_error_response(
    const ErrorResponse & response
)
{
    // 确保错误消息不超过最大长度限制
    if (response.message.size() > DefaultMaxStringBytes) {
        return std::unexpected(make_error(
            ProtocolErrorCode::ResourceLimitExceeded,
            "error message exceeds the configured limit"
        ));
    }

    core::io::BufferByteWriter buffer {MaxPayloadSize};
    Writer writer {buffer};

    // 在 payload 中写入错误码
    if (auto result = writer.write_u16(response.code); !result) {
        return std::unexpected(std::move(result.error()));
    }
    // 在 payload 中写入错误消息
    if (auto result = writer.write_string(response.message); !result) {
        return std::unexpected(std::move(result.error()));
    }

    return buffer.take_bytes();
}

std::expected<ErrorResponse, ProtocolError> decode_error_response(
    std::span<const std::byte> payload,
    ProtocolDecodeLimits limits
)
{
    // 确保 payload 大小不超过最大限制
    if (auto size = ensure_payload_size(payload); !size) {
        return std::unexpected(std::move(size.error()));
    }

    core::io::BufferByteReader source {payload};
    auto reader = make_reader(source, payload, limits.max_string_bytes);

    // 读取错误码
    auto code = reader.read_u16();
    if (!code) {
        return std::unexpected(std::move(code.error()));
    }
    // 读取错误消息
    auto message = reader.read_string();
    if (!message) {
        return std::unexpected(std::move(message.error()));
    }

    // 确保读取器已经读取完所有数据
    if (auto done = ensure_done(reader, "error response"); !done) {
        return std::unexpected(std::move(done.error()));
    }

    return ErrorResponse {
        .code = *code,
        .message = std::move(*message),
    };
}

} // namespace litedb::protocol
