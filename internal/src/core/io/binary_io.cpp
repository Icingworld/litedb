#include "core/io/binary_io.hpp"

#include <bit>
#include <concepts>
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
 * @brief 写入小端编码的整数
 * @tparam T 整数类型
 * @param writer 写入器
 * @param value 整数值
 * @return 结果
 */
template <typename T>
[[nodiscard]]
std::expected<void, IoError> write_little_endian_integer(BinaryWriter & writer, T value)
{
    static_assert(std::is_integral_v<T>);
    using Unsigned = std::make_unsigned_t<T>;
    auto data = static_cast<Unsigned>(value);
    for (std::size_t index = 0; index < sizeof(T); ++index) {
        auto result = writer.write_u8(static_cast<std::uint8_t>((data >> (index * 8U)) & 0xffU));
        if (!result.has_value()) {
            return std::unexpected(std::move(result.error()));
        }
    }
    return {};
}

/**
 * @brief 读取小端编码的整数
 * @tparam T 整数类型
 * @param reader 读取器
 * @return 整数值
 */
template <typename T>
[[nodiscard]]
std::expected<T, IoError> read_little_endian_integer(BinaryReader & reader)
{
    static_assert(std::is_integral_v<T>);
    using Unsigned = std::make_unsigned_t<T>;
    Unsigned result = 0;
    for (std::size_t index = 0; index < sizeof(T); ++index) {
        auto byte = reader.read_u8();
        if (!byte.has_value()) {
            return std::unexpected(std::move(byte.error()));
        }
        result |= static_cast<Unsigned>(*byte) << (index * 8U);
    }
    return static_cast<T>(result);
}

/**
 * @brief 按 IEEE 754 小端格式写入浮点数
 * @tparam T 浮点数类型
 * @param writer 写入器
 * @param value 浮点数值
 * @return 结果
 * @note 浮点数必须使用 IEEE 754 小端格式
 */
template <std::floating_point T>
[[nodiscard]]
std::expected<void, IoError> write_little_endian_floating(
    BinaryWriter& writer,
    T value
)
{
    static_assert(std::numeric_limits<T>::is_iec559, "floating-point type must use IEEE 754");

    if constexpr (std::same_as<T, float>) {
        static_assert(sizeof(float) == sizeof(std::uint32_t));

        const auto bits = std::bit_cast<std::uint32_t>(value);
        return writer.write_u32(bits);
    }
    else if constexpr (std::same_as<T, double>) {
        static_assert(sizeof(double) == sizeof(std::uint64_t));

        const auto bits = std::bit_cast<std::uint64_t>(value);
        return writer.write_u64(bits);
    }
    else {
        static_assert(
            std::same_as<T, float> || std::same_as<T, double>,
            "unsupported floating-point type"
        );
    }
}

/**
 * @brief 按 IEEE 754 小端格式读取浮点数
 * @tparam T 浮点数类型
 * @param reader 读取器
 * @return 浮点数值
 * @note 浮点数必须使用 IEEE 754 小端格式
 */
template <std::floating_point T>
[[nodiscard]]
std::expected<T, IoError> read_little_endian_floating(
    BinaryReader& reader
)
{
    static_assert(std::numeric_limits<T>::is_iec559, "floating-point type must use IEEE 754");

    if constexpr (std::same_as<T, float>) {
        static_assert(sizeof(float) == sizeof(std::uint32_t));

        auto bits = reader.read_u32();
        if (!bits) {
            return std::unexpected(std::move(bits.error()));
        }

        return std::bit_cast<float>(*bits);
    }
    else if constexpr (std::same_as<T, double>) {
        static_assert(sizeof(double) == sizeof(std::uint64_t));

        auto bits = reader.read_u64();
        if (!bits) {
            return std::unexpected(std::move(bits.error()));
        }

        return std::bit_cast<double>(*bits);
    }
    else {
        static_assert(
            std::same_as<T, float> || std::same_as<T, double>,
            "unsupported floating-point type"
        );
    }
}

} // namespace

BinaryWriter::BinaryWriter(ByteWriter & writer) noexcept
    : writer_(writer)
{
}

std::expected<void, IoError> BinaryWriter::write_u8(std::uint8_t value)
{
    return write_bytes(&value, sizeof(value));
}

std::expected<void, IoError> BinaryWriter::write_u16(std::uint16_t value)
{
    return write_little_endian_integer(*this, value);
}

std::expected<void, IoError> BinaryWriter::write_u32(std::uint32_t value)
{
    return write_little_endian_integer(*this, value);
}

std::expected<void, IoError> BinaryWriter::write_u64(std::uint64_t value)
{
    return write_little_endian_integer(*this, value);
}

std::expected<void, IoError> BinaryWriter::write_i32(std::int32_t value)
{
    return write_little_endian_integer(*this, value);
}

std::expected<void, IoError> BinaryWriter::write_i64(std::int64_t value)
{
    return write_little_endian_integer(*this, value);
}

std::expected<void, IoError> BinaryWriter::write_f32(float value)
{
    return write_little_endian_floating(*this, value);
}

std::expected<void, IoError> BinaryWriter::write_f64(double value)
{
    return write_little_endian_floating(*this, value);
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
        return std::unexpected(std::move(result.error()));
    }
    if (!value.empty()) {
        auto result = write_bytes(value.data(), value.size());
        if (!result.has_value()) {
            return std::unexpected(std::move(result.error()));
        }
    }
    return {};
}

std::expected<void, IoError> BinaryWriter::write_value(const common::Value & value)
{
    return std::visit(
        [this](const auto & data) -> std::expected<void, IoError> {
            using T = std::decay_t<decltype(data)>;
            if constexpr (std::is_same_v<T, common::NullValue>) {
                return write_u8(static_cast<std::uint8_t>(EncodedValueKind::Null));
            }
            if constexpr (std::is_same_v<T, bool>) {
                auto type_result = write_u8(static_cast<std::uint8_t>(EncodedValueKind::Boolean));
                if (!type_result.has_value()) {
                    return std::unexpected(std::move(type_result.error()));
                }
                return write_u8(data ? 1U : 0U);
            }
            if constexpr (std::is_same_v<T, std::int32_t>) {
                auto type_result = write_u8(static_cast<std::uint8_t>(EncodedValueKind::Integer));
                if (!type_result.has_value()) {
                    return std::unexpected(std::move(type_result.error()));
                }
                return write_i32(data);
            }
            if constexpr (std::is_same_v<T, std::int64_t>) {
                auto type_result = write_u8(static_cast<std::uint8_t>(EncodedValueKind::BigInt));
                if (!type_result.has_value()) {
                    return std::unexpected(std::move(type_result.error()));
                }
                return write_i64(data);
            }
            if constexpr (std::is_same_v<T, float>) {
                auto type_result = write_u8(static_cast<std::uint8_t>(EncodedValueKind::Float));
                if (!type_result.has_value()) {
                    return std::unexpected(std::move(type_result.error()));
                }
                return write_f32(data);
            }
            if constexpr (std::is_same_v<T, double>) {
                auto type_result = write_u8(static_cast<std::uint8_t>(EncodedValueKind::Double));
                if (!type_result.has_value()) {
                    return std::unexpected(std::move(type_result.error()));
                }
                return write_f64(data);
            }
            if constexpr (std::is_same_v<T, std::string>) {
                auto type_result = write_u8(static_cast<std::uint8_t>(EncodedValueKind::String));
                if (!type_result.has_value()) {
                    return std::unexpected(std::move(type_result.error()));
                }
                return write_string(data);
            }
            if constexpr (std::is_same_v<T, common::VectorValue>) {
                if (data.size() > std::numeric_limits<std::uint32_t>::max()) {
                    return std::unexpected(
                        make_io_error(IoErrorCode::ValueTooLarge, "vector is too large to encode")
                    );
                }
                auto type_result = write_u8(static_cast<std::uint8_t>(EncodedValueKind::Vector));
                if (!type_result.has_value()) {
                    return std::unexpected(std::move(type_result.error()));
                }
                if (data.empty()) {
                    return write_u32(0);
                }
                auto size_result = write_u32(static_cast<std::uint32_t>(data.size()));
                if (!size_result.has_value()) {
                    return std::unexpected(std::move(size_result.error()));
                }
                for (std::size_t index = 0; index + 1 < data.size(); ++index) {
                    auto element_result = write_f64(data[index]);
                    if (!element_result.has_value()) {
                        return std::unexpected(std::move(element_result.error()));
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
    
    return writer_.write_bytes(bytes);
}

BinaryReader::BinaryReader(ByteReader & reader) noexcept
    : reader_(reader)
{
}

std::expected<std::uint8_t, IoError> BinaryReader::read_u8()
{
    std::uint8_t value = 0;
    auto read = read_exact_bytes(&value, sizeof(value));
    if (!read.has_value()) {
        return std::unexpected(std::move(read.error()));
    }
    return value;
}

std::expected<std::uint16_t, IoError> BinaryReader::read_u16()
{
    return read_little_endian_integer<std::uint16_t>(*this);
}

std::expected<std::uint32_t, IoError> BinaryReader::read_u32()
{
    return read_little_endian_integer<std::uint32_t>(*this);
}

std::expected<std::uint64_t, IoError> BinaryReader::read_u64()
{
    return read_little_endian_integer<std::uint64_t>(*this);
}

std::expected<std::int32_t, IoError> BinaryReader::read_i32()
{
    return read_little_endian_integer<std::int32_t>(*this);
}

std::expected<std::int64_t, IoError> BinaryReader::read_i64()
{
    return read_little_endian_integer<std::int64_t>(*this);
}

std::expected<float, IoError> BinaryReader::read_f32()
{
    return read_little_endian_floating<float>(*this);
}

std::expected<double, IoError> BinaryReader::read_f64()
{
    return read_little_endian_floating<double>(*this);
}

std::expected<std::string, IoError> BinaryReader::read_string(std::size_t max_size)
{
    auto size = read_u32();
    if (!size.has_value()) {
        return std::unexpected(std::move(size.error()));
    }
    if (*size > max_size) {
        return std::unexpected(make_io_error(IoErrorCode::ValueTooLarge, "string is too large to decode"));
    }
    std::string value(*size, '\0');
    if (*size != 0) {
        auto read = read_exact_bytes(value.data(), value.size());
        if (!read.has_value()) {
            return std::unexpected(std::move(read.error()));
        }
    }
    return value;
}

std::expected<common::Value, IoError> BinaryReader::read_value()
{
    auto kind_byte = read_u8();
    if (!kind_byte.has_value()) {
        return std::unexpected(std::move(kind_byte.error()));
    }
    const auto kind = static_cast<EncodedValueKind>(*kind_byte);
    switch (kind) {
    case EncodedValueKind::Null:
        return common::Value::null();
    case EncodedValueKind::Boolean: {
        auto value = read_u8();
        if (!value.has_value()) {
            return std::unexpected(std::move(value.error()));
        }
        return common::Value {*value != 0};
    }
    case EncodedValueKind::Integer: {
        auto value = read_i32();
        if (!value.has_value()) {
            return std::unexpected(std::move(value.error()));
        }
        return common::Value {*value};
    }
    case EncodedValueKind::BigInt: {
        auto value = read_i64();
        if (!value.has_value()) {
            return std::unexpected(std::move(value.error()));
        }
        return common::Value {*value};
    }
    case EncodedValueKind::Float: {
        auto value = read_f32();
        if (!value.has_value()) {
            return std::unexpected(std::move(value.error()));
        }
        return common::Value {*value};
    }
    case EncodedValueKind::Double: {
        auto value = read_f64();
        if (!value.has_value()) {
            return std::unexpected(std::move(value.error()));
        }
        return common::Value {*value};
    }
    case EncodedValueKind::String: {
        auto value = read_string();
        if (!value.has_value()) {
            return std::unexpected(std::move(value.error()));
        }
        return common::Value {std::move(*value)};
    }
    case EncodedValueKind::Vector: {
        auto count = read_u32();
        if (!count.has_value()) {
            return std::unexpected(std::move(count.error()));
        }
        if (*count > std::numeric_limits<std::uint32_t>::max()) {
            return std::unexpected(make_io_error(IoErrorCode::ValueTooLarge, "vector is too large to decode"));
        }
        common::VectorValue values;
        values.reserve(*count);
        for (std::uint32_t index = 0; index < *count; ++index) {
            auto value = read_f64();
            if (!value.has_value()) {
                return std::unexpected(std::move(value.error()));
            }
            values.push_back(*value);
        }
        return common::Value {std::move(values)};
    }
    }

    return std::unexpected(make_io_error(IoErrorCode::InvalidData, "invalid encoded value kind"));
}

std::expected<void, IoError> BinaryReader::read_exact_bytes(void * data, std::size_t size)
{
    return reader_.read_exact(std::span {
        static_cast<std::byte *>(data),
        size,
    });
}

} // namespace litedb::core::io
