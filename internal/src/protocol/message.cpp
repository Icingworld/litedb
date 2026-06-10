#include "protocol/message.hpp"

#include <bit>
#include <cstring>
#include <limits>
#include <span>
#include <type_traits>
#include <utility>

#include "core/common/logical_id.hpp"
#include "core/schema/value.hpp"

namespace litedb::protocol
{

namespace
{

class Writer
{
public:
    [[nodiscard]]
    std::vector<std::uint8_t> finish() &&
    {
        return std::move(buffer_);
    }

    void write_u8(std::uint8_t value)
    {
        buffer_.push_back(value);
    }

    void write_u16(std::uint16_t value)
    {
        buffer_.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
        buffer_.push_back(static_cast<std::uint8_t>(value & 0xffU));
    }

    void write_u32(std::uint32_t value)
    {
        for (int shift = 24; shift >= 0; shift -= 8) {
            buffer_.push_back(static_cast<std::uint8_t>((value >> shift) & 0xffU));
        }
    }

    void write_u64(std::uint64_t value)
    {
        for (int shift = 56; shift >= 0; shift -= 8) {
            buffer_.push_back(static_cast<std::uint8_t>((value >> shift) & 0xffU));
        }
    }

    void write_bytes(std::span<const std::uint8_t> bytes)
    {
        buffer_.insert(buffer_.end(), bytes.begin(), bytes.end());
    }

    void write_string(std::string_view value)
    {
        write_u32(checked_size(value.size()));
        const auto * data = reinterpret_cast<const std::uint8_t *>(value.data());
        write_bytes(std::span {data, value.size()});
    }

    void write_float(float value)
    {
        write_u32(std::bit_cast<std::uint32_t>(value));
    }

    void write_double(double value)
    {
        write_u64(std::bit_cast<std::uint64_t>(value));
    }

private:
    static std::uint32_t checked_size(std::size_t size)
    {
        if (size > std::numeric_limits<std::uint32_t>::max()) {
            return std::numeric_limits<std::uint32_t>::max();
        }
        return static_cast<std::uint32_t>(size);
    }

    std::vector<std::uint8_t> buffer_;
};

class Reader
{
public:
    explicit Reader(std::span<const std::uint8_t> bytes) noexcept
        : bytes_(bytes)
    {
    }

    [[nodiscard]]
    bool done() const noexcept
    {
        return offset_ == bytes_.size();
    }

    [[nodiscard]]
    std::expected<std::uint8_t, ProtocolError> read_u8()
    {
        if (!can_read(1)) {
            return unexpected_end();
        }
        return bytes_[offset_++];
    }

    [[nodiscard]]
    std::expected<std::uint16_t, ProtocolError> read_u16()
    {
        if (!can_read(2)) {
            return unexpected_end();
        }
        const auto value = static_cast<std::uint16_t>((bytes_[offset_] << 8U) | bytes_[offset_ + 1]);
        offset_ += 2;
        return value;
    }

    [[nodiscard]]
    std::expected<std::uint32_t, ProtocolError> read_u32()
    {
        if (!can_read(4)) {
            return unexpected_end();
        }
        std::uint32_t value {0};
        for (int index = 0; index < 4; ++index) {
            value = (value << 8U) | bytes_[offset_ + index];
        }
        offset_ += 4;
        return value;
    }

    [[nodiscard]]
    std::expected<std::uint64_t, ProtocolError> read_u64()
    {
        if (!can_read(8)) {
            return unexpected_end();
        }
        std::uint64_t value {0};
        for (int index = 0; index < 8; ++index) {
            value = (value << 8U) | bytes_[offset_ + index];
        }
        offset_ += 8;
        return value;
    }

    [[nodiscard]]
    std::expected<std::string, ProtocolError> read_string()
    {
        auto size = read_u32();
        if (!size.has_value()) {
            return std::unexpected(size.error());
        }
        if (!can_read(size.value())) {
            return unexpected_end();
        }
        std::string value {
            reinterpret_cast<const char *>(bytes_.data() + offset_),
            static_cast<std::size_t>(size.value())
        };
        offset_ += size.value();
        return value;
    }

    [[nodiscard]]
    std::expected<float, ProtocolError> read_float()
    {
        auto bits = read_u32();
        if (!bits.has_value()) {
            return std::unexpected(bits.error());
        }
        return std::bit_cast<float>(bits.value());
    }

    [[nodiscard]]
    std::expected<double, ProtocolError> read_double()
    {
        auto bits = read_u64();
        if (!bits.has_value()) {
            return std::unexpected(bits.error());
        }
        return std::bit_cast<double>(bits.value());
    }

private:
    [[nodiscard]]
    bool can_read(std::size_t size) const noexcept
    {
        return size <= bytes_.size() - offset_;
    }

    [[nodiscard]]
    static std::unexpected<ProtocolError> unexpected_end()
    {
        return std::unexpected(ProtocolError {ProtocolErrorCode::UnexpectedEnd, "unexpected end of payload"});
    }

    std::span<const std::uint8_t> bytes_;
    std::size_t offset_ {0};
};

[[nodiscard]]
std::unexpected<ProtocolError> invalid_payload(std::string message)
{
    return std::unexpected(ProtocolError {ProtocolErrorCode::InvalidPayload, std::move(message)});
}

[[nodiscard]]
std::uint64_t to_u64(std::size_t value)
{
    return static_cast<std::uint64_t>(value);
}

[[nodiscard]]
std::uint32_t to_u32(std::size_t value)
{
    return static_cast<std::uint32_t>(value);
}

void write_logical_type(Writer & writer, const core::common::LogicalType & type)
{
    writer.write_u8(static_cast<std::uint8_t>(type.id));
    writer.write_u8(type.parameter.has_value() ? 1 : 0);
    if (type.parameter.has_value()) {
        writer.write_u64(to_u64(type.parameter.value()));
    }
}

[[nodiscard]]
std::expected<core::common::LogicalType, ProtocolError> read_logical_type(Reader & reader)
{
    auto type_id = reader.read_u8();
    if (!type_id.has_value()) {
        return std::unexpected(type_id.error());
    }

    auto has_parameter = reader.read_u8();
    if (!has_parameter.has_value()) {
        return std::unexpected(has_parameter.error());
    }

    std::optional<std::size_t> parameter;
    if (has_parameter.value() != 0) {
        auto value = reader.read_u64();
        if (!value.has_value()) {
            return std::unexpected(value.error());
        }
        parameter = static_cast<std::size_t>(value.value());
    }

    if (type_id.value() > static_cast<std::uint8_t>(core::common::LogicalTypeId::Vector)) {
        return invalid_payload("invalid logical type id");
    }

    return core::common::LogicalType {
        static_cast<core::common::LogicalTypeId>(type_id.value()),
        parameter,
    };
}

void write_value(Writer & writer, const core::schema::Value & value)
{
    using core::schema::NullValue;
    using core::schema::VectorValue;

    std::visit(
        [&](const auto & data) {
            using T = std::decay_t<decltype(data)>;
            if constexpr (std::is_same_v<T, NullValue>) {
                writer.write_u8(static_cast<std::uint8_t>(core::common::LogicalTypeId::Null));
            } else if constexpr (std::is_same_v<T, bool>) {
                writer.write_u8(static_cast<std::uint8_t>(core::common::LogicalTypeId::Boolean));
                writer.write_u8(data ? 1 : 0);
            } else if constexpr (std::is_same_v<T, std::int32_t>) {
                writer.write_u8(static_cast<std::uint8_t>(core::common::LogicalTypeId::Integer));
                writer.write_u32(std::bit_cast<std::uint32_t>(data));
            } else if constexpr (std::is_same_v<T, std::int64_t>) {
                writer.write_u8(static_cast<std::uint8_t>(core::common::LogicalTypeId::BigInt));
                writer.write_u64(std::bit_cast<std::uint64_t>(data));
            } else if constexpr (std::is_same_v<T, float>) {
                writer.write_u8(static_cast<std::uint8_t>(core::common::LogicalTypeId::Float));
                writer.write_float(data);
            } else if constexpr (std::is_same_v<T, double>) {
                writer.write_u8(static_cast<std::uint8_t>(core::common::LogicalTypeId::Double));
                writer.write_double(data);
            } else if constexpr (std::is_same_v<T, std::string>) {
                writer.write_u8(static_cast<std::uint8_t>(core::common::LogicalTypeId::Varchar));
                writer.write_string(data);
            } else if constexpr (std::is_same_v<T, VectorValue>) {
                writer.write_u8(static_cast<std::uint8_t>(core::common::LogicalTypeId::Vector));
                writer.write_u32(to_u32(data.size()));
                for (const auto item : data) {
                    writer.write_double(item);
                }
            }
        },
        value.data()
    );
}

[[nodiscard]]
std::expected<core::schema::Value, ProtocolError> read_value(Reader & reader)
{
    auto tag = reader.read_u8();
    if (!tag.has_value()) {
        return std::unexpected(tag.error());
    }

    const auto type = static_cast<core::common::LogicalTypeId>(tag.value());
    switch (type) {
    case core::common::LogicalTypeId::Null:
        return core::schema::Value::null();
    case core::common::LogicalTypeId::Boolean: {
        auto value = reader.read_u8();
        if (!value.has_value()) {
            return std::unexpected(value.error());
        }
        return core::schema::Value {value.value() != 0};
    }
    case core::common::LogicalTypeId::Integer: {
        auto value = reader.read_u32();
        if (!value.has_value()) {
            return std::unexpected(value.error());
        }
        return core::schema::Value {std::bit_cast<std::int32_t>(value.value())};
    }
    case core::common::LogicalTypeId::BigInt: {
        auto value = reader.read_u64();
        if (!value.has_value()) {
            return std::unexpected(value.error());
        }
        return core::schema::Value {std::bit_cast<std::int64_t>(value.value())};
    }
    case core::common::LogicalTypeId::Float: {
        auto value = reader.read_float();
        if (!value.has_value()) {
            return std::unexpected(value.error());
        }
        return core::schema::Value {value.value()};
    }
    case core::common::LogicalTypeId::Double: {
        auto value = reader.read_double();
        if (!value.has_value()) {
            return std::unexpected(value.error());
        }
        return core::schema::Value {value.value()};
    }
    case core::common::LogicalTypeId::Varchar: {
        auto value = reader.read_string();
        if (!value.has_value()) {
            return std::unexpected(value.error());
        }
        return core::schema::Value {std::move(value.value())};
    }
    case core::common::LogicalTypeId::Vector: {
        auto count = reader.read_u32();
        if (!count.has_value()) {
            return std::unexpected(count.error());
        }
        core::schema::VectorValue values;
        values.reserve(count.value());
        for (std::uint32_t index = 0; index < count.value(); ++index) {
            auto value = reader.read_double();
            if (!value.has_value()) {
                return std::unexpected(value.error());
            }
            values.push_back(value.value());
        }
        return core::schema::Value {std::move(values)};
    }
    }

    return invalid_payload("invalid value tag");
}

} // namespace

std::vector<std::uint8_t> encode_frame(const Frame & frame)
{
    Writer writer;
    writer.write_u32(static_cast<std::uint32_t>(frame.payload.size()));
    writer.write_u16(frame.header.version);
    writer.write_u16(static_cast<std::uint16_t>(frame.header.kind));
    writer.write_u64(frame.header.request_id);
    writer.write_bytes(frame.payload);
    return std::move(writer).finish();
}

std::expected<FrameHeader, ProtocolError> decode_frame_header(const std::uint8_t * data, std::size_t size)
{
    if (size < FrameHeaderSize) {
        return std::unexpected(ProtocolError {ProtocolErrorCode::InvalidFrame, "frame header is too short"});
    }

    Reader reader {std::span {data, FrameHeaderSize}};
    auto payload_size = reader.read_u32();
    auto version = reader.read_u16();
    auto kind = reader.read_u16();
    auto request_id = reader.read_u64();
    if (!payload_size.has_value() || !version.has_value() || !kind.has_value() || !request_id.has_value()) {
        return std::unexpected(ProtocolError {ProtocolErrorCode::InvalidFrame, "frame header is invalid"});
    }
    if (version.value() != ProtocolVersion) {
        return std::unexpected(ProtocolError {ProtocolErrorCode::InvalidVersion, "unsupported protocol version"});
    }
    if (kind.value() < static_cast<std::uint16_t>(MessageKind::ExecuteSqlRequest)
        || kind.value() > static_cast<std::uint16_t>(MessageKind::PongResponse)) {
        return std::unexpected(ProtocolError {ProtocolErrorCode::InvalidMessageKind, "invalid message kind"});
    }

    return FrameHeader {
        .payload_size = payload_size.value(),
        .version = version.value(),
        .kind = static_cast<MessageKind>(kind.value()),
        .request_id = request_id.value(),
    };
}

std::expected<Frame, ProtocolError> decode_frame(const std::uint8_t * data, std::size_t size)
{
    auto header = decode_frame_header(data, size);
    if (!header.has_value()) {
        return std::unexpected(header.error());
    }
    if (size < FrameHeaderSize + header->payload_size) {
        return std::unexpected(ProtocolError {ProtocolErrorCode::UnexpectedEnd, "frame payload is too short"});
    }

    std::vector<std::uint8_t> payload {
        data + FrameHeaderSize,
        data + FrameHeaderSize + header->payload_size,
    };
    return Frame {.header = header.value(), .payload = std::move(payload)};
}

std::vector<std::uint8_t> encode_execute_sql_request(std::string_view sql)
{
    Writer writer;
    writer.write_string(sql);
    return std::move(writer).finish();
}

std::expected<ExecuteSqlRequest, ProtocolError> decode_execute_sql_request(const std::vector<std::uint8_t> & payload)
{
    Reader reader {payload};
    auto sql = reader.read_string();
    if (!sql.has_value()) {
        return std::unexpected(sql.error());
    }
    if (!reader.done()) {
        return invalid_payload("execute SQL request has trailing bytes");
    }
    return ExecuteSqlRequest {std::move(sql.value())};
}

std::vector<std::uint8_t> encode_execute_sql_response(const core::executor::ExecutionResult & result)
{
    Writer writer;
    writer.write_u8(static_cast<std::uint8_t>(result.kind));
    writer.write_u64(to_u64(result.affected_rows));
    writer.write_u8(result.selected_database_name.has_value() ? 1 : 0);
    if (result.selected_database_name.has_value()) {
        writer.write_string(result.selected_database_name.value());
    }

    writer.write_u32(to_u32(result.columns.size()));
    for (const auto & column : result.columns) {
        writer.write_string(column.name);
        write_logical_type(writer, column.type);
    }

    writer.write_u32(to_u32(result.rows.size()));
    for (const auto & row : result.rows) {
        writer.write_u32(to_u32(row.values.size()));
        for (const auto & value : row.values) {
            write_value(writer, value);
        }
    }

    return std::move(writer).finish();
}

std::expected<core::executor::ExecutionResult, ProtocolError> decode_execute_sql_response(
    const std::vector<std::uint8_t> & payload
)
{
    Reader reader {payload};
    core::executor::ExecutionResult result;

    auto kind = reader.read_u8();
    if (!kind.has_value()) {
        return std::unexpected(kind.error());
    }
    if (kind.value() > static_cast<std::uint8_t>(core::executor::ExecutionResultKind::UseDatabase)) {
        return invalid_payload("invalid execution result kind");
    }
    result.kind = static_cast<core::executor::ExecutionResultKind>(kind.value());

    auto affected_rows = reader.read_u64();
    if (!affected_rows.has_value()) {
        return std::unexpected(affected_rows.error());
    }
    result.affected_rows = static_cast<std::size_t>(affected_rows.value());

    auto has_selected_database_name = reader.read_u8();
    if (!has_selected_database_name.has_value()) {
        return std::unexpected(has_selected_database_name.error());
    }
    if (has_selected_database_name.value() != 0) {
        auto selected_database_name = reader.read_string();
        if (!selected_database_name.has_value()) {
            return std::unexpected(selected_database_name.error());
        }
        result.selected_database_name = std::move(selected_database_name.value());
    }

    auto column_count = reader.read_u32();
    if (!column_count.has_value()) {
        return std::unexpected(column_count.error());
    }
    result.columns.reserve(column_count.value());
    for (std::uint32_t index = 0; index < column_count.value(); ++index) {
        auto name = reader.read_string();
        if (!name.has_value()) {
            return std::unexpected(name.error());
        }
        auto type = read_logical_type(reader);
        if (!type.has_value()) {
            return std::unexpected(type.error());
        }
        result.columns.push_back(core::executor::ExecutionColumn {
            .name = std::move(name.value()),
            .type = type.value(),
        });
    }

    auto row_count = reader.read_u32();
    if (!row_count.has_value()) {
        return std::unexpected(row_count.error());
    }
    result.rows.reserve(row_count.value());
    for (std::uint32_t row_index = 0; row_index < row_count.value(); ++row_index) {
        auto value_count = reader.read_u32();
        if (!value_count.has_value()) {
            return std::unexpected(value_count.error());
        }
        core::executor::ExecutionRow row;
        row.values.reserve(value_count.value());
        for (std::uint32_t value_index = 0; value_index < value_count.value(); ++value_index) {
            auto value = read_value(reader);
            if (!value.has_value()) {
                return std::unexpected(value.error());
            }
            row.values.push_back(std::move(value.value()));
        }
        result.rows.push_back(std::move(row));
    }

    if (!reader.done()) {
        return invalid_payload("execute SQL response has trailing bytes");
    }

    return result;
}

std::vector<std::uint8_t> encode_error_response(const ErrorResponse & response)
{
    Writer writer;
    writer.write_u16(response.code);
    writer.write_string(response.message);
    return std::move(writer).finish();
}

std::expected<ErrorResponse, ProtocolError> decode_error_response(const std::vector<std::uint8_t> & payload)
{
    Reader reader {payload};
    auto code = reader.read_u16();
    if (!code.has_value()) {
        return std::unexpected(code.error());
    }
    auto message = reader.read_string();
    if (!message.has_value()) {
        return std::unexpected(message.error());
    }
    if (!reader.done()) {
        return invalid_payload("error response has trailing bytes");
    }
    return ErrorResponse {
        .code = code.value(),
        .message = std::move(message.value()),
    };
}

} // namespace litedb::protocol
