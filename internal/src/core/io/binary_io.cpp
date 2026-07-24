#include "core/io/binary_io.hpp"

#include <array>
#include <bit>
#include <concepts>
#include <limits>
#include <type_traits>
#include <utility>

#include "core/io/io_helper.hpp"

namespace litedb::core::io
{

namespace
{

template <std::integral T>
using UnsignedInteger = std::make_unsigned_t<T>;

template <std::integral T>
std::array<std::byte, sizeof(T)> encode_little_endian(T value) noexcept
{
    std::array<std::byte, sizeof(T)> bytes {};
    auto data = static_cast<UnsignedInteger<T>>(value);
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        bytes[index] = static_cast<std::byte>((data >> (index * 8U)) & 0xffU);
    }
    return bytes;
}

template <std::integral T>
T decode_little_endian(const std::array<std::byte, sizeof(T)> & bytes) noexcept
{
    UnsignedInteger<T> value = 0;
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        value |= static_cast<UnsignedInteger<T>>(std::to_integer<std::uint8_t>(bytes[index]))
                 << (index * 8U);
    }
    if constexpr (std::is_signed_v<T>) {
        return std::bit_cast<T>(value);
    }
    return value;
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
    const auto bytes = encode_little_endian(value);
    return write_bytes(bytes.data(), bytes.size());
}

std::expected<void, IoError> BinaryWriter::write_u32(std::uint32_t value)
{
    const auto bytes = encode_little_endian(value);
    return write_bytes(bytes.data(), bytes.size());
}

std::expected<void, IoError> BinaryWriter::write_u64(std::uint64_t value)
{
    const auto bytes = encode_little_endian(value);
    return write_bytes(bytes.data(), bytes.size());
}

std::expected<void, IoError> BinaryWriter::write_i32(std::int32_t value)
{
    const auto bytes = encode_little_endian(value);
    return write_bytes(bytes.data(), bytes.size());
}

std::expected<void, IoError> BinaryWriter::write_i64(std::int64_t value)
{
    const auto bytes = encode_little_endian(value);
    return write_bytes(bytes.data(), bytes.size());
}

std::expected<void, IoError> BinaryWriter::write_f32(float value)
{
    static_assert(std::numeric_limits<float>::is_iec559);
    return write_u32(std::bit_cast<std::uint32_t>(value));
}

std::expected<void, IoError> BinaryWriter::write_f64(double value)
{
    static_assert(std::numeric_limits<double>::is_iec559);
    return write_u64(std::bit_cast<std::uint64_t>(value));
}

std::expected<void, IoError> BinaryWriter::write_string(std::string_view value)
{
    if (value.size() > std::numeric_limits<std::uint32_t>::max()) {
        return std::unexpected(make_io_error(
            IoErrorCode::ValueTooLarge,
            "string is too large to encode"
        ));
    }
    if (auto result = write_u32(static_cast<std::uint32_t>(value.size())); !result) {
        return std::unexpected(std::move(result.error()));
    }
    if (value.empty()) {
        return {};
    }
    return write_bytes(value.data(), value.size());
}

std::expected<void, IoError> BinaryWriter::write_bytes(const void * data, std::size_t size)
{
    return writer_.write_bytes(std::span {
        static_cast<const std::byte *>(data),
        size,
    });
}

BinaryReader::BinaryReader(ByteReader & reader, BinaryDecodeLimits limits) noexcept
    : reader_(reader)
    , limits_(limits)
    , remaining_bytes_(limits.max_total_bytes)
{
}

std::expected<std::uint8_t, IoError> BinaryReader::read_u8()
{
    std::uint8_t value = 0;
    if (auto read = read_exact_bytes(&value, sizeof(value)); !read) {
        return std::unexpected(std::move(read.error()));
    }
    return value;
}

std::expected<std::uint16_t, IoError> BinaryReader::read_u16()
{
    std::array<std::byte, sizeof(std::uint16_t)> bytes {};
    if (auto read = read_exact_bytes(bytes.data(), bytes.size()); !read) {
        return std::unexpected(std::move(read.error()));
    }
    return decode_little_endian<std::uint16_t>(bytes);
}

std::expected<std::uint32_t, IoError> BinaryReader::read_u32()
{
    std::array<std::byte, sizeof(std::uint32_t)> bytes {};
    if (auto read = read_exact_bytes(bytes.data(), bytes.size()); !read) {
        return std::unexpected(std::move(read.error()));
    }
    return decode_little_endian<std::uint32_t>(bytes);
}

std::expected<std::uint64_t, IoError> BinaryReader::read_u64()
{
    std::array<std::byte, sizeof(std::uint64_t)> bytes {};
    if (auto read = read_exact_bytes(bytes.data(), bytes.size()); !read) {
        return std::unexpected(std::move(read.error()));
    }
    return decode_little_endian<std::uint64_t>(bytes);
}

std::expected<std::int32_t, IoError> BinaryReader::read_i32()
{
    std::array<std::byte, sizeof(std::int32_t)> bytes {};
    if (auto read = read_exact_bytes(bytes.data(), bytes.size()); !read) {
        return std::unexpected(std::move(read.error()));
    }
    return decode_little_endian<std::int32_t>(bytes);
}

std::expected<std::int64_t, IoError> BinaryReader::read_i64()
{
    std::array<std::byte, sizeof(std::int64_t)> bytes {};
    if (auto read = read_exact_bytes(bytes.data(), bytes.size()); !read) {
        return std::unexpected(std::move(read.error()));
    }
    return decode_little_endian<std::int64_t>(bytes);
}

std::expected<float, IoError> BinaryReader::read_f32()
{
    auto bits = read_u32();
    if (!bits) {
        return std::unexpected(std::move(bits.error()));
    }
    static_assert(std::numeric_limits<float>::is_iec559);
    return std::bit_cast<float>(*bits);
}

std::expected<double, IoError> BinaryReader::read_f64()
{
    auto bits = read_u64();
    if (!bits) {
        return std::unexpected(std::move(bits.error()));
    }
    static_assert(std::numeric_limits<double>::is_iec559);
    return std::bit_cast<double>(*bits);
}

std::expected<std::string, IoError> BinaryReader::read_string()
{
    auto size = read_u32();
    if (!size) {
        return std::unexpected(std::move(size.error()));
    }
    if (*size > limits_.max_string_bytes) {
        return std::unexpected(make_io_error(
            IoErrorCode::ValueTooLarge,
            "string exceeds the configured decode limit"
        ));
    }
    if (*size > remaining_bytes_) {
        return std::unexpected(make_io_error(
            IoErrorCode::UnexpectedEof,
            "string length exceeds the remaining binary data"
        ));
    }

    std::string value(*size, '\0');
    if (!value.empty()) {
        if (auto read = read_exact_bytes(value.data(), value.size()); !read) {
            return std::unexpected(std::move(read.error()));
        }
    }
    return value;
}

std::uint64_t BinaryReader::remaining_bytes() const noexcept
{
    return remaining_bytes_;
}

std::expected<void, IoError> BinaryReader::read_exact_bytes(void * data, std::size_t size)
{
    if (size > remaining_bytes_) {
        return std::unexpected(make_io_error(
            IoErrorCode::UnexpectedEof,
            "read exceeds the configured binary data budget"
        ));
    }
    auto result = reader_.read_exact(std::span {
        static_cast<std::byte *>(data),
        size,
    });
    if (!result) {
        return std::unexpected(std::move(result.error()));
    }
    remaining_bytes_ -= size;
    return {};
}

} // namespace litedb::core::io
