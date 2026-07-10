#include "core/io/binary_io.hpp"

#include <array>
#include <cstring>
#include <expected>
#include <limits>
#include <type_traits>
#include <utility>

#include "core/io/io_helper.hpp"

namespace litedb::core::io
{

namespace
{

enum class EncodedValueKind : std::uint8_t
{
    Null = 0,
    Boolean = 1,
    Integer = 2,
    BigInt = 3,
    Float = 4,
    Double = 5,
    String = 6,
    Vector = 7,
};

/**
 * @brief 写入小端编码的整�? * @tparam T 整数类型
 * @param writer 写入�? * @param value 整数�? * @return 结果
 */
template <typename T>
std::expected<void, IoError> write_little_endian(BinaryWriter & writer, T value)
{
    static_assert(std::is_integral_v<T>);
    using Unsigned = std::make_unsigned_t<T>;
    auto data = static_cast<Unsigned>(value);
    for (std::size_t index = 0; index < sizeof(T); ++index) {
        auto result = writer.write_u8(static_cast<std::uint8_t>((data >> (index * 8U)) & 0xffU));
        if (!result.has_value()) {
            return std::unexpected(result.error());
        }
    }
    return {};
}

template <typename T>
std::expected<T, IoError> read_little_endian(BinaryReader & reader)
{
    static_assert(std::is_integral_v<T>);
    using Unsigned = std::make_unsigned_t<T>;
    Unsigned result = 0;
    for (std::size_t index = 0; index < sizeof(T); ++index) {
        auto byte = reader.read_u8();
        if (!byte.has_value()) {
            return std::unexpected(byte.error());
        }
        result |= static_cast<Unsigned>(byte.value()) << (index * 8U);
    }
    return static_cast<T>(result);
}

template <typename T>
T bit_cast_from_bytes(const std::byte * bytes)
{
    T value {};
    std::memcpy(&value, bytes, sizeof(T));
    return value;
}

/**
 * @brief 写入浮点�? * @tparam T 浮点数类�? * @param writer 写入�? * @param value 浮点数�? * @return 结果
 */
template <typename T>
std::expected<void, IoError> write_floating(BinaryWriter & writer, T value)
{
    std::array<std::byte, sizeof(T)> bytes {};
    std::memcpy(bytes.data(), &value, sizeof(T));
    for (const auto byte : bytes) {
        auto result = writer.write_u8(std::to_integer<std::uint8_t>(byte));
        if (!result.has_value()) {
            return std::unexpected(result.error());
        }
    }
    return {};
}

template <typename T>
std::expected<T, IoError> read_floating(BinaryReader & reader)
{
    std::array<std::byte, sizeof(T)> bytes {};
    auto read = reader.read_bytes(bytes.data(), bytes.size());
    if (!read.has_value()) {
        return std::unexpected(read.error());
    }
    if (!read.value()) {
        return std::unexpected(
            make_io_error(IoErrorCode::UnexpectedEof, "unexpected end of floating point value")
        );
    }
    return bit_cast_from_bytes<T>(bytes.data());
}

} // namespace

BinaryWriter::BinaryWriter(ByteWriter & writer) noexcept
    : writer_(&writer)
{
}

std::expected<void, IoError> BinaryWriter::write_u8(std::uint8_t value)
{
    return write_bytes(&value, sizeof(value));
}

std::expected<void, IoError> BinaryWriter::write_u16(std::uint16_t value)
{
    return write_little_endian(*this, value);
}

std::expected<void, IoError> BinaryWriter::write_u32(std::uint32_t value)
{
    return write_little_endian(*this, value);
}

std::expected<void, IoError> BinaryWriter::write_u64(std::uint64_t value)
{
    return write_little_endian(*this, value);
}

std::expected<void, IoError> BinaryWriter::write_i32(std::int32_t value)
{
    return write_little_endian(*this, value);
}

std::expected<void, IoError> BinaryWriter::write_i64(std::int64_t value)
{
    return write_little_endian(*this, value);
}

std::expected<void, IoError> BinaryWriter::write_f32(float value)
{
    return write_floating(*this, value);
}

std::expected<void, IoError> BinaryWriter::write_f64(double value)
{
    return write_floating(*this, value);
}

std::expected<void, IoError> BinaryWriter::write_string(const std::string & value)
{
    if (value.size() > std::numeric_limits<std::uint32_t>::max()) {
        return std::unexpected(
            make_io_error(IoErrorCode::ValueTooLarge, "string is too large to encode")
        );
    }
    auto result = write_u32(static_cast<std::uint32_t>(value.size()));
    if (!result.has_value()) {
        return std::unexpected(result.error());
    }
    if (!value.empty()) {
        auto result = write_bytes(value.data(), value.size());
        if (!result.has_value()) {
            return std::unexpected(result.error());
        }
    }
    return {};
}

std::expected<void, IoError> BinaryWriter::write_value(const schema::Value & value)
{
    return std::visit(
        [this](const auto & data) -> std::expected<void, IoError> {
            using T = std::decay_t<decltype(data)>;
            if constexpr (std::is_same_v<T, schema::NullValue>) {
                return write_u8(static_cast<std::uint8_t>(EncodedValueKind::Null));
            }
            if constexpr (std::is_same_v<T, bool>) {
                auto type_result = write_u8(static_cast<std::uint8_t>(EncodedValueKind::Boolean));
                if (!type_result.has_value()) {
                    return std::unexpected(type_result.error());
                }
                return write_u8(data ? 1U : 0U);
            }
            if constexpr (std::is_same_v<T, std::int32_t>) {
                auto type_result = write_u8(static_cast<std::uint8_t>(EncodedValueKind::Integer));
                if (!type_result.has_value()) {
                    return std::unexpected(type_result.error());
                }
                return write_i32(data);
            }
            if constexpr (std::is_same_v<T, std::int64_t>) {
                auto type_result = write_u8(static_cast<std::uint8_t>(EncodedValueKind::BigInt));
                if (!type_result.has_value()) {
                    return std::unexpected(type_result.error());
                }
                return write_i64(data);
            }
            if constexpr (std::is_same_v<T, float>) {
                auto type_result = write_u8(static_cast<std::uint8_t>(EncodedValueKind::Float));
                if (!type_result.has_value()) {
                    return std::unexpected(type_result.error());
                }
                return write_f32(data);
            }
            if constexpr (std::is_same_v<T, double>) {
                auto type_result = write_u8(static_cast<std::uint8_t>(EncodedValueKind::Double));
                if (!type_result.has_value()) {
                    return std::unexpected(type_result.error());
                }
                return write_f64(data);
            }
            if constexpr (std::is_same_v<T, std::string>) {
                auto type_result = write_u8(static_cast<std::uint8_t>(EncodedValueKind::String));
                if (!type_result.has_value()) {
                    return std::unexpected(type_result.error());
                }
                return write_string(data);
            }
            if constexpr (std::is_same_v<T, schema::VectorValue>) {
                if (data.size() > std::numeric_limits<std::uint32_t>::max()) {
                    return std::unexpected(
                        make_io_error(IoErrorCode::ValueTooLarge, "vector is too large to encode")
                    );
                }
                auto type_result = write_u8(static_cast<std::uint8_t>(EncodedValueKind::Vector));
                if (!type_result.has_value()) {
                    return std::unexpected(type_result.error());
                }
                if (data.empty()) {
                    return write_u32(0);
                }
                auto size_result = write_u32(static_cast<std::uint32_t>(data.size()));
                if (!size_result.has_value()) {
                    return std::unexpected(size_result.error());
                }
                for (std::size_t index = 0; index + 1 < data.size(); ++index) {
                    auto element_result = write_f64(data[index]);
                    if (!element_result.has_value()) {
                        return std::unexpected(element_result.error());
                    }
                }
                return write_f64(data.back());
            }
        },
        value.data()
    );
}

std::expected<void, IoError> BinaryWriter::write_bytes(const void * data, std::size_t size)
{
    auto bytes = std::span {
        static_cast<const std::byte *>(data),
        size,
    };
    
    return writer_->write_bytes(bytes);
}

BinaryReader::BinaryReader(ByteReader & reader) noexcept
    : reader_(&reader)
{
}

std::expected<bool, IoError> BinaryReader::read_u32(std::uint32_t & value)
{
    std::array<std::byte, sizeof(std::uint32_t)> bytes {};
    auto read = reader_->read_bytes(bytes);
    if (!read.has_value()) {
        return std::unexpected(read.error());
    }
    if (read.value() != bytes.size()) {
        return false;
    }

    std::uint32_t result = 0;
    for (std::size_t index = 0; index < sizeof(std::uint32_t); ++index) {
        result |= static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[index])) << (index * 8U);
    }
    value = result;
    return true;
}

std::expected<bool, IoError> BinaryReader::read_bytes(void * data, std::size_t size)
{
    auto read = reader_->read_bytes(std::span {static_cast<std::byte *>(data), size});
    if (!read.has_value()) {
        return std::unexpected(read.error());
    }
    return read.value() == size;
}

std::expected<std::uint8_t, IoError> BinaryReader::read_u8()
{
    std::uint8_t value = 0;
    auto read = read_exact_bytes(&value, sizeof(value));
    if (!read.has_value()) {
        return std::unexpected(read.error());
    }
    return value;
}

std::expected<std::uint16_t, IoError> BinaryReader::read_u16()
{
    return read_little_endian<std::uint16_t>(*this);
}

std::expected<std::uint32_t, IoError> BinaryReader::read_u32()
{
    return read_little_endian<std::uint32_t>(*this);
}

std::expected<std::uint64_t, IoError> BinaryReader::read_u64()
{
    return read_little_endian<std::uint64_t>(*this);
}

std::expected<std::int32_t, IoError> BinaryReader::read_i32()
{
    return read_little_endian<std::int32_t>(*this);
}

std::expected<std::int64_t, IoError> BinaryReader::read_i64()
{
    return read_little_endian<std::int64_t>(*this);
}

std::expected<float, IoError> BinaryReader::read_f32()
{
    return read_floating<float>(*this);
}

std::expected<double, IoError> BinaryReader::read_f64()
{
    return read_floating<double>(*this);
}

std::expected<std::string, IoError> BinaryReader::read_string()
{
    const auto size = read_u32();
    if (!size.has_value()) {
        return std::unexpected(size.error());
    }
    std::string value(size.value(), '\0');
    if (size.value() != 0) {
        auto read = read_exact_bytes(value.data(), value.size());
        if (!read.has_value()) {
            return std::unexpected(read.error());
        }
    }
    return value;
}

std::expected<schema::Value, IoError> BinaryReader::read_value()
{
    const auto kind_byte = read_u8();
    if (!kind_byte.has_value()) {
        return std::unexpected(kind_byte.error());
    }
    const auto kind = static_cast<EncodedValueKind>(kind_byte.value());
    switch (kind) {
    case EncodedValueKind::Null:
        return schema::Value::null();
    case EncodedValueKind::Boolean: {
        auto value = read_u8();
        if (!value.has_value()) {
            return std::unexpected(value.error());
        }
        return schema::Value {value.value() != 0};
    }
    case EncodedValueKind::Integer: {
        auto value = read_i32();
        if (!value.has_value()) {
            return std::unexpected(value.error());
        }
        return schema::Value {value.value()};
    }
    case EncodedValueKind::BigInt: {
        auto value = read_i64();
        if (!value.has_value()) {
            return std::unexpected(value.error());
        }
        return schema::Value {value.value()};
    }
    case EncodedValueKind::Float: {
        auto value = read_f32();
        if (!value.has_value()) {
            return std::unexpected(value.error());
        }
        return schema::Value {value.value()};
    }
    case EncodedValueKind::Double: {
        auto value = read_f64();
        if (!value.has_value()) {
            return std::unexpected(value.error());
        }
        return schema::Value {value.value()};
    }
    case EncodedValueKind::String: {
        auto value = read_string();
        if (!value.has_value()) {
            return std::unexpected(value.error());
        }
        return schema::Value {std::move(value.value())};
    }
    case EncodedValueKind::Vector: {
        const auto count = read_u32();
        if (!count.has_value()) {
            return std::unexpected(count.error());
        }
        schema::VectorValue values;
        values.reserve(count.value());
        for (std::uint32_t index = 0; index < count.value(); ++index) {
            auto value = read_f64();
            if (!value.has_value()) {
                return std::unexpected(value.error());
            }
            values.push_back(value.value());
        }
        return schema::Value {std::move(values)};
    }
    }

    return std::unexpected(make_io_error(IoErrorCode::InvalidData, "invalid encoded value kind"));
}

std::expected<void, IoError> BinaryReader::read_exact_bytes(void * data, std::size_t size)
{
    auto read = read_bytes(data, size);
    if (!read.has_value()) {
        return std::unexpected(read.error());
    }
    if (!read.value()) {
        return std::unexpected(make_io_error(IoErrorCode::UnexpectedEof, "unexpected end of binary data"));
    }
    return {};
}

} // namespace litedb::core::io
