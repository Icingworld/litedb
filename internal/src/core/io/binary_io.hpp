#pragma once

#include <cstdint>
#include <expected>
#include <string>

#include "core/io/io_error.hpp"
#include "core/io/byte_reader.hpp"
#include "core/io/byte_writer.hpp"
#include "core/schema/value.hpp"

namespace litedb::core::io
{

/**
 * @brief 二进制写入器
 */
class BinaryWriter
{
public:
    explicit BinaryWriter(ByteWriter & writer) noexcept;

public:
    /**
     * @brief 写入字节
     * @param value 字节值
     * @return 结果
     */
    std::expected<void, IoError> write_u8(std::uint8_t value);

    /**
     * @brief 写入 16 位无符号整数
     * @param value 16 位无符号整数值
     * @return 结果
     */
    std::expected<void, IoError> write_u16(std::uint16_t value);

    /**
     * @brief 写入 32 位无符号整数
     * @param value 32 位无符号整数值
     * @return 结果
     */
    std::expected<void, IoError> write_u32(std::uint32_t value);

    /**
     * @brief 写入 64 位无符号整数
     * @param value 64 位无符号整数值
     * @return 结果
     */
    std::expected<void, IoError> write_u64(std::uint64_t value);

    /**
     * @brief 写入 32 位有符号整数
     * @param value 32 位有符号整数值
     * @return 结果
     */
    std::expected<void, IoError> write_i32(std::int32_t value);

    /**
     * @brief 写入 64 位有符号整数
     * @param value 64 位有符号整数值
     * @return 结果
     */
    std::expected<void, IoError> write_i64(std::int64_t value);

    /**
     * @brief 写入 32 位浮点数
     * @param value 32 位浮点数值
     * @return 结果
     */
    std::expected<void, IoError> write_f32(float value);

    /**
     * @brief 写入 64 位浮点数
     * @param value 64 位浮点数值
     * @return 结果
     */
    std::expected<void, IoError> write_f64(double value);

    /**
     * @brief 写入字符串
     * @param value 字符串值
     * @return 结果
     */
    std::expected<void, IoError> write_string(const std::string & value);

    /**
     * @brief 写入值
     * @param value 值
     * @return 结果
     */
    std::expected<void, IoError> write_value(const schema::Value & value);

private:
    /**
     * @brief 写入原始字节
     * @param data 字节数据指针
     * @param size 字节数据大小
     * @return 结果
     */
    std::expected<void, IoError> write_bytes(const void * data, std::size_t size);

private:
    ByteWriter & writer_;            ///< 字节写入器
};

/**
 * @brief 二进制读取器
 */
class BinaryReader
{
public:
    explicit BinaryReader(ByteReader & reader) noexcept;

public:
    /**
     * @brief 读取 8 位无符号整数
     * @return 读取结果
     */
    std::expected<std::uint8_t, IoError> read_u8();

    /**
     * @brief 读取 16 位无符号整数
     * @return 读取结果
     */
    std::expected<std::uint16_t, IoError> read_u16();

    /**
     * @brief 读取 32 位无符号整数
     * @return 读取结果
     */
    std::expected<std::uint32_t, IoError> read_u32();

    /**
     * @brief 读取 64 位无符号整数
     * @return 读取结果
     */
    std::expected<std::uint64_t, IoError> read_u64();

    /**
     * @brief 读取 32 位有符号整数
     * @return 读取结果
     */
    std::expected<std::int32_t, IoError> read_i32();

    /**
     * @brief 读取 64 位有符号整数
     * @return 读取结果
     */
    std::expected<std::int64_t, IoError> read_i64();

    /**
     * @brief 读取 32 位浮点数
     * @return 读取结果
     */
    std::expected<float, IoError> read_f32();

    /**
     * @brief 读取 64 位浮点数
     * @return 读取结果
     */
    std::expected<double, IoError> read_f64();

    /**
     * @brief 读取字符串
     * @return 读取结果
     */
    std::expected<std::string, IoError> read_string();

    /**
     * @brief 读取值
     * @return 读取结果
     */
    std::expected<schema::Value, IoError> read_value();

private:
    /**
     * @brief 读取指定数量的字节，不足时返回错误
     * @param data 输出缓冲区
     * @param size 要读取的字节数
     * @return 结果
     */
    std::expected<void, IoError> read_exact_bytes(void * data, std::size_t size);

private:
    ByteReader & reader_;            ///< 字节读取器
};

} // namespace litedb::core::io
