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

// 二进制解码资源限制
struct BinaryDecodeLimits
{
    std::uint64_t max_total_bytes; // 最多可读取的总字节数
    std::uint32_t max_string_bytes; // 单个字符串的最大字节数
};

// 二进制写入器
// 通过模板传入字节序，支持 std::endian::little 和 std::endian::big
template <std::endian E>
class BasicBinaryWriter
{
public:
    explicit BasicBinaryWriter(ByteWriter & writer) noexcept;

public:
    // 写入一个 8 位无符号整数
    [[nodiscard]]
    std::expected<void, IoError> write_u8(std::uint8_t value);

    // 写入一个 16 位无符号整数
    [[nodiscard]]
    std::expected<void, IoError> write_u16(std::uint16_t value);

    // 写入一个 32 位无符号整数
    [[nodiscard]]
    std::expected<void, IoError> write_u32(std::uint32_t value);

    // 写入一个 64 位无符号整数
    [[nodiscard]]
    std::expected<void, IoError> write_u64(std::uint64_t value);

    // 写入一个 32 位有符号整数
    [[nodiscard]]
    std::expected<void, IoError> write_i32(std::int32_t value);

    // 写入一个 64 位有符号整数
    [[nodiscard]]
    std::expected<void, IoError> write_i64(std::int64_t value);

    // 写入一个 32 位浮点数
    [[nodiscard]]
    std::expected<void, IoError> write_f32(float value);

    // 写入一个 64 位浮点数
    [[nodiscard]]
    std::expected<void, IoError> write_f64(double value);

    // 写入一个字符串
    // 如果在写入长度后，写入字符串时因为空间不足或其他原因而失败
    // 调用方应终止当前编码流程并丢弃本次编码结果
    [[nodiscard]]
    std::expected<void, IoError> write_string(std::string_view value);

private:
    // 写入字节数据
    [[nodiscard]]
    std::expected<void, IoError> write_bytes(const void * data, std::size_t size);

private:
    ByteWriter & writer_;
};

// 有界二进制读取器
// 通过模板传入字节序，支持 std::endian::little 和 std::endian::big
template <std::endian E>
class BasicBinaryReader
{
public:
    BasicBinaryReader(ByteReader & reader, BinaryDecodeLimits limits) noexcept;

public:
    // 读取一个 8 位无符号整数
    [[nodiscard]]
    std::expected<std::uint8_t, IoError> read_u8();

    // 读取一个 16 位无符号整数
    [[nodiscard]]
    std::expected<std::uint16_t, IoError> read_u16();

    // 读取一个 32 位无符号整数
    [[nodiscard]]
    std::expected<std::uint32_t, IoError> read_u32();

    // 读取一个 64 位无符号整数
    [[nodiscard]]
    std::expected<std::uint64_t, IoError> read_u64();

    // 读取一个 32 位有符号整数
    [[nodiscard]]
    std::expected<std::int32_t, IoError> read_i32();

    // 读取一个 64 位有符号整数
    [[nodiscard]]
    std::expected<std::int64_t, IoError> read_i64();

    // 读取一个 32 位浮点数
    [[nodiscard]]
    std::expected<float, IoError> read_f32();

    // 读取一个 64 位浮点数
    [[nodiscard]]
    std::expected<double, IoError> read_f64();

    // 读取一个字符串
    [[nodiscard]]
    std::expected<std::string, IoError> read_string();

    // 获取剩余读取预算
    [[nodiscard]]
    std::uint64_t remaining_bytes() const noexcept;

private:
    // 读取精确的字节数据
    [[nodiscard]]
    std::expected<void, IoError> read_exact_bytes(void * data, std::size_t size);

private:
    ByteReader & reader_;
    BinaryDecodeLimits limits_;
    std::uint64_t remaining_bytes_;
};

// 大小端读写器类型别名
using LittleEndianBinaryWriter = BasicBinaryWriter<std::endian::little>;
using LittleEndianBinaryReader = BasicBinaryReader<std::endian::little>;
using BigEndianBinaryWriter = BasicBinaryWriter<std::endian::big>;
using BigEndianBinaryReader = BasicBinaryReader<std::endian::big>;

// 显式实例化模板类
// 大小端类型有限，避免隐式实例化带来的代码体积膨胀
extern template class BasicBinaryWriter<std::endian::little>;
extern template class BasicBinaryWriter<std::endian::big>;
extern template class BasicBinaryReader<std::endian::little>;
extern template class BasicBinaryReader<std::endian::big>;

} // namespace litedb::core::io
