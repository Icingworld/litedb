#pragma once

#include <cstdint>
#include <istream>
#include <ostream>
#include <string>

#include "core/schema/value.hpp"

namespace litedb::core::persistence
{

/**
 * @brief 二进制写入器
 */
class BinaryWriter
{
public:
    explicit BinaryWriter(std::ostream & out) noexcept;

public:
    void write_u8(std::uint8_t value);
    void write_u16(std::uint16_t value);
    void write_u32(std::uint32_t value);
    void write_u64(std::uint64_t value);
    void write_i32(std::int32_t value);
    void write_i64(std::int64_t value);
    void write_f32(float value);
    void write_f64(double value);
    void write_string(const std::string & value);
    void write_value(const schema::Value & value);

private:
    void write_bytes(const void * data, std::size_t size);

private:
    std::ostream * out_;            ///< 输出流
};

/**
 * @brief 二进制读取器
 */
class BinaryReader
{
public:
    explicit BinaryReader(std::istream & in) noexcept;

public:
    [[nodiscard]]
    bool try_read_u32(std::uint32_t & value);

    [[nodiscard]]
    bool try_read_bytes(void * data, std::size_t size);

    std::uint8_t read_u8();
    std::uint16_t read_u16();
    std::uint32_t read_u32();
    std::uint64_t read_u64();
    std::int32_t read_i32();
    std::int64_t read_i64();
    float read_f32();
    double read_f64();
    std::string read_string();
    schema::Value read_value();

private:
    void read_bytes(void * data, std::size_t size);

private:
    std::istream * in_;                ///< 输入流
};

} // namespace litedb::core::persistence
