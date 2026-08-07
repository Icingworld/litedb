#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

#include "core/io/byte_reader.hpp"
#include "core/io/byte_writer.hpp"
#include "core/io/io_error.hpp"

namespace litedb::core::io
{

/**
 * @brief 二进制解码资源限制
 */
struct BinaryDecodeLimits
{
    std::uint64_t max_total_bytes;       // 最多可读取的总字节数
    std::uint32_t max_string_bytes;      // 单个字符串的最大字节数
};

/**
 * @brief 二进制写入器
 * @tparam E 字节序（std::endian::little 或 std::endian::big）
 */
template <std::endian E>
class BasicBinaryWriter
{
public:
    explicit BasicBinaryWriter(ByteWriter & writer) noexcept;

public:
    [[nodiscard]] std::expected<void, IoError> write_u8(std::uint8_t value);
    [[nodiscard]] std::expected<void, IoError> write_u16(std::uint16_t value);
    [[nodiscard]] std::expected<void, IoError> write_u32(std::uint32_t value);
    [[nodiscard]] std::expected<void, IoError> write_u64(std::uint64_t value);
    [[nodiscard]] std::expected<void, IoError> write_i32(std::int32_t value);
    [[nodiscard]] std::expected<void, IoError> write_i64(std::int64_t value);
    [[nodiscard]] std::expected<void, IoError> write_f32(float value);
    [[nodiscard]] std::expected<void, IoError> write_f64(double value);
    [[nodiscard]] std::expected<void, IoError> write_string(std::string_view value);

private:
    [[nodiscard]]
    std::expected<void, IoError> write_bytes(const void * data, std::size_t size);

private:
    ByteWriter & writer_;       // 字节写入器
};

/**
 * @brief 有界二进制读取器
 * @tparam E 字节序（std::endian::little 或 std::endian::big）
 */
template <std::endian E>
class BasicBinaryReader
{
public:
    BasicBinaryReader(ByteReader & reader, BinaryDecodeLimits limits) noexcept;

public:
    [[nodiscard]] std::expected<std::uint8_t, IoError> read_u8();
    [[nodiscard]] std::expected<std::uint16_t, IoError> read_u16();
    [[nodiscard]] std::expected<std::uint32_t, IoError> read_u32();
    [[nodiscard]] std::expected<std::uint64_t, IoError> read_u64();
    [[nodiscard]] std::expected<std::int32_t, IoError> read_i32();
    [[nodiscard]] std::expected<std::int64_t, IoError> read_i64();
    [[nodiscard]] std::expected<float, IoError> read_f32();
    [[nodiscard]] std::expected<double, IoError> read_f64();
    [[nodiscard]] std::expected<std::string, IoError> read_string();

    [[nodiscard]]
    std::uint64_t remaining_bytes() const noexcept;

private:
    [[nodiscard]]
    std::expected<void, IoError> read_exact_bytes(void * data, std::size_t size);

private:
    ByteReader & reader_;                   // 字节读取器
    BinaryDecodeLimits limits_;             // 解码限制
    std::uint64_t remaining_bytes_;         // 剩余读取预算
};

using LittleEndianBinaryWriter = BasicBinaryWriter<std::endian::little>;
using LittleEndianBinaryReader = BasicBinaryReader<std::endian::little>;
using BigEndianBinaryWriter = BasicBinaryWriter<std::endian::big>;
using BigEndianBinaryReader = BasicBinaryReader<std::endian::big>;

/**
 * @brief 小端二进制写入器（兼容别名）
 */
using BinaryWriter = LittleEndianBinaryWriter;

/**
 * @brief 有界小端二进制读取器（兼容别名）
 */
using BinaryReader = LittleEndianBinaryReader;

extern template class BasicBinaryWriter<std::endian::little>;
extern template class BasicBinaryWriter<std::endian::big>;
extern template class BasicBinaryReader<std::endian::little>;
extern template class BasicBinaryReader<std::endian::big>;

} // namespace litedb::core::io
