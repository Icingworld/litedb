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

// 无符号整数类型别名
// T 必须是整数类型
// 该别名的作用是得到与 T 宽度相同的无符号整数类型
// 例如：std::int32_t -> std::uint32_t
template <std::integral T>
using UnsignedInteger = std::make_unsigned_t<T>;

// 编码整数为字节数组
template <std::endian E, std::integral T>
std::array<std::byte, sizeof(T)> encode_integer(T value) noexcept
{
    std::array<std::byte, sizeof(T)> bytes {};
    auto data = static_cast<UnsignedInteger<T>>(value);
    constexpr std::size_t last = sizeof(T) - 1;
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        const std::size_t position = (E == std::endian::little) ? index : last - index;
        bytes[position] = static_cast<std::byte>((data >> (index * 8U)) & 0xffU);
    }
    return bytes;
}

// 解码字节数组为整数
template <std::endian E, std::integral T>
T decode_integer(const std::array<std::byte, sizeof(T)> & bytes) noexcept
{
    UnsignedInteger<T> value = 0;
    constexpr std::size_t last = sizeof(T) - 1;
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        const std::size_t position = (E == std::endian::little) ? index : last - index;
        value |= static_cast<UnsignedInteger<T>>(std::to_integer<std::uint8_t>(bytes[position]))
                 << (index * 8U);
    }
    if constexpr (std::is_signed_v<T>) {
        return std::bit_cast<T>(value);
    }
    return value;
}

} // namespace

template <std::endian E>
BasicBinaryWriter<E>::BasicBinaryWriter(ByteWriter & writer) noexcept
    : writer_(writer)
{
}

template <std::endian E>
std::expected<void, IoError> BasicBinaryWriter<E>::write_u8(std::uint8_t value)
{
    return write_bytes(&value, sizeof(value));
}

template <std::endian E>
std::expected<void, IoError> BasicBinaryWriter<E>::write_u16(std::uint16_t value)
{
    const auto bytes = encode_integer<E>(value);
    return write_bytes(bytes.data(), bytes.size());
}

template <std::endian E>
std::expected<void, IoError> BasicBinaryWriter<E>::write_u32(std::uint32_t value)
{
    const auto bytes = encode_integer<E>(value);
    return write_bytes(bytes.data(), bytes.size());
}

template <std::endian E>
std::expected<void, IoError> BasicBinaryWriter<E>::write_u64(std::uint64_t value)
{
    const auto bytes = encode_integer<E>(value);
    return write_bytes(bytes.data(), bytes.size());
}

template <std::endian E>
std::expected<void, IoError> BasicBinaryWriter<E>::write_i32(std::int32_t value)
{
    const auto bytes = encode_integer<E>(value);
    return write_bytes(bytes.data(), bytes.size());
}

template <std::endian E>
std::expected<void, IoError> BasicBinaryWriter<E>::write_i64(std::int64_t value)
{
    const auto bytes = encode_integer<E>(value);
    return write_bytes(bytes.data(), bytes.size());
}

template <std::endian E>
std::expected<void, IoError> BasicBinaryWriter<E>::write_f32(float value)
{
    static_assert(std::numeric_limits<float>::is_iec559);
    return write_u32(std::bit_cast<std::uint32_t>(value));
}

template <std::endian E>
std::expected<void, IoError> BasicBinaryWriter<E>::write_f64(double value)
{
    static_assert(std::numeric_limits<double>::is_iec559);
    return write_u64(std::bit_cast<std::uint64_t>(value));
}

template <std::endian E>
std::expected<void, IoError> BasicBinaryWriter<E>::write_string(std::string_view value)
{
    if (value.size() > std::numeric_limits<std::uint32_t>::max()) [[unlikely]] {
        return std::unexpected(make_io_error(
            IoErrorCode::ValueTooLarge,
            "string is too large to encode"
        ));
    }
    if (auto result = write_u32(static_cast<std::uint32_t>(value.size())); !result) [[unlikely]] {
        return std::unexpected(std::move(result.error()));
    }
    if (value.empty()) {
        return {};
    }
    return write_bytes(value.data(), value.size());
}

template <std::endian E>
std::expected<void, IoError> BasicBinaryWriter<E>::write_bytes(const void * data, std::size_t size)
{
    return writer_.write_bytes(std::span {
        static_cast<const std::byte *>(data),
        size,
    });
}

template <std::endian E>
BasicBinaryReader<E>::BasicBinaryReader(ByteReader & reader, BinaryDecodeLimits limits) noexcept
    : reader_(reader)
    , limits_(limits)
    , remaining_bytes_(limits.max_total_bytes)
{
}

template <std::endian E>
std::expected<std::uint8_t, IoError> BasicBinaryReader<E>::read_u8()
{
    std::uint8_t value = 0;
    if (auto read = read_exact_bytes(&value, sizeof(value)); !read) [[unlikely]] {
        return std::unexpected(std::move(read.error()));
    }
    return value;
}

template <std::endian E>
std::expected<std::uint16_t, IoError> BasicBinaryReader<E>::read_u16()
{
    std::array<std::byte, sizeof(std::uint16_t)> bytes {};
    if (auto read = read_exact_bytes(bytes.data(), bytes.size()); !read) [[unlikely]] {
        return std::unexpected(std::move(read.error()));
    }
    return decode_integer<E, std::uint16_t>(bytes);
}

template <std::endian E>
std::expected<std::uint32_t, IoError> BasicBinaryReader<E>::read_u32()
{
    std::array<std::byte, sizeof(std::uint32_t)> bytes {};
    if (auto read = read_exact_bytes(bytes.data(), bytes.size()); !read) [[unlikely]] {
        return std::unexpected(std::move(read.error()));
    }
    return decode_integer<E, std::uint32_t>(bytes);
}

template <std::endian E>
std::expected<std::uint64_t, IoError> BasicBinaryReader<E>::read_u64()
{
    std::array<std::byte, sizeof(std::uint64_t)> bytes {};
    if (auto read = read_exact_bytes(bytes.data(), bytes.size()); !read) [[unlikely]] {
        return std::unexpected(std::move(read.error()));
    }
    return decode_integer<E, std::uint64_t>(bytes);
}

template <std::endian E>
std::expected<std::int32_t, IoError> BasicBinaryReader<E>::read_i32()
{
    std::array<std::byte, sizeof(std::int32_t)> bytes {};
    if (auto read = read_exact_bytes(bytes.data(), bytes.size()); !read) [[unlikely]] {
        return std::unexpected(std::move(read.error()));
    }
    return decode_integer<E, std::int32_t>(bytes);
}

template <std::endian E>
std::expected<std::int64_t, IoError> BasicBinaryReader<E>::read_i64()
{
    std::array<std::byte, sizeof(std::int64_t)> bytes {};
    if (auto read = read_exact_bytes(bytes.data(), bytes.size()); !read) [[unlikely]] {
        return std::unexpected(std::move(read.error()));
    }
    return decode_integer<E, std::int64_t>(bytes);
}

template <std::endian E>
std::expected<float, IoError> BasicBinaryReader<E>::read_f32()
{
    auto bits = read_u32();
    if (!bits) [[unlikely]] {
        return std::unexpected(std::move(bits.error()));
    }
    static_assert(std::numeric_limits<float>::is_iec559);
    return std::bit_cast<float>(*bits);
}

template <std::endian E>
std::expected<double, IoError> BasicBinaryReader<E>::read_f64()
{
    auto bits = read_u64();
    if (!bits) [[unlikely]] {
        return std::unexpected(std::move(bits.error()));
    }
    static_assert(std::numeric_limits<double>::is_iec559);
    return std::bit_cast<double>(*bits);
}

template <std::endian E>
std::expected<std::string, IoError> BasicBinaryReader<E>::read_string()
{
    auto size = read_u32();
    if (!size) [[unlikely]] {
        return std::unexpected(std::move(size.error()));
    }
    if (*size > limits_.max_string_bytes) [[unlikely]] {
        return std::unexpected(make_io_error(
            IoErrorCode::ValueTooLarge,
            "string exceeds the configured decode limit"
        ));
    }
    if (*size > remaining_bytes_) [[unlikely]] {
        return std::unexpected(make_io_error(
            IoErrorCode::UnexpectedEof,
            "string length exceeds the remaining binary data"
        ));
    }

    std::string value(*size, '\0');
    if (!value.empty()) {
        if (auto read = read_exact_bytes(value.data(), value.size()); !read) [[unlikely]] {
            return std::unexpected(std::move(read.error()));
        }
    }
    return value;
}

template <std::endian E>
std::uint64_t BasicBinaryReader<E>::remaining_bytes() const noexcept
{
    return remaining_bytes_;
}

template <std::endian E>
std::expected<void, IoError> BasicBinaryReader<E>::read_exact_bytes(void * data, std::size_t size)
{
    if (size > remaining_bytes_) [[unlikely]] {
        return std::unexpected(make_io_error(
            IoErrorCode::UnexpectedEof,
            "read exceeds the configured binary data budget"
        ));
    }
    auto result = reader_.read_exact(std::span {
        static_cast<std::byte *>(data),
        size,
    });
    if (!result) [[unlikely]] {
        return std::unexpected(std::move(result.error()));
    }
    remaining_bytes_ -= size;
    return {};
}

template class BasicBinaryWriter<std::endian::little>;
template class BasicBinaryWriter<std::endian::big>;
template class BasicBinaryReader<std::endian::little>;
template class BasicBinaryReader<std::endian::big>;

} // namespace litedb::core::io
