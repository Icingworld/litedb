#include "core/persistence/binary_io.hpp"

#include <cstring>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace litedb::core::persistence
{

namespace
{

/**
 * @brief 编码值类型
 */
enum class EncodedValueKind : std::uint8_t
{
    Null = 0,                         ///< 空值
    Boolean = 1,                      ///< 布尔值
    Integer = 2,                      ///< 整数
    BigInt = 3,                       ///< 大整数
    Float = 4,                        ///< 浮点数
    Double = 5,                       ///< 双精度浮点数
    String = 6,                       ///< 字符串
    Vector = 7,                       ///< 向量
};

/**
 * @brief 写入小端编码
 * @param out 输出流
 * @param value 值
 */
template <typename T>
void write_little_endian(std::ostream & out, T value)
{
    static_assert(std::is_integral_v<T>);
    using Unsigned = std::make_unsigned_t<T>;
    auto data = static_cast<Unsigned>(value);
    for (std::size_t index = 0; index < sizeof(T); ++index) {
        const auto byte = static_cast<char>((data >> (index * 8U)) & 0xffU);
        out.write(&byte, 1);
    }
    if (!out) {
        throw std::runtime_error("failed to write binary data");
    }
}

/**
 * @brief 读取小端编码
 * @param in 输入流
 * @return 值
 */
template <typename T>
T read_little_endian(std::istream & in)
{
    static_assert(std::is_integral_v<T>);
    using Unsigned = std::make_unsigned_t<T>;
    Unsigned result = 0;
    for (std::size_t index = 0; index < sizeof(T); ++index) {
        char byte = 0;
        in.read(&byte, 1);
        if (!in) {
            throw std::runtime_error("unexpected end of binary data");
        }
        result |= static_cast<Unsigned>(static_cast<unsigned char>(byte)) << (index * 8U);
    }
    return static_cast<T>(result);
}

/**
 * @brief 从字节数组转换为值
 * @param bytes 字节数组
 * @return 值
 */
template <typename T>
T bit_cast_from_bytes(const char * bytes)
{
    T value {};
    std::memcpy(&value, bytes, sizeof(T));
    return value;
}

/**
 * @brief 写入浮点数
 * @param out 输出流
 * @param value 值
 */
template <typename T>
void write_floating(std::ostream & out, T value)
{
    char bytes[sizeof(T)] {};
    std::memcpy(bytes, &value, sizeof(T));
    out.write(bytes, sizeof(T));
    if (!out) {
        throw std::runtime_error("failed to write floating point value");
    }
}

/**
 * @brief 读取浮点数
 * @param in 输入流
 * @return 值
 */
template <typename T>
T read_floating(std::istream & in)
{
    char bytes[sizeof(T)] {};
    in.read(bytes, sizeof(T));
    if (!in) {
        throw std::runtime_error("unexpected end of floating point value");
    }
    return bit_cast_from_bytes<T>(bytes);
}

} // namespace

BinaryWriter::BinaryWriter(std::ostream & out) noexcept
    : out_(&out)
{
}

void BinaryWriter::write_u8(std::uint8_t value)
{
    write_bytes(&value, sizeof(value));
}

void BinaryWriter::write_u16(std::uint16_t value)
{
    write_little_endian(*out_, value);
}

void BinaryWriter::write_u32(std::uint32_t value)
{
    write_little_endian(*out_, value);
}

void BinaryWriter::write_u64(std::uint64_t value)
{
    write_little_endian(*out_, value);
}

void BinaryWriter::write_i32(std::int32_t value)
{
    write_little_endian(*out_, value);
}

void BinaryWriter::write_i64(std::int64_t value)
{
    write_little_endian(*out_, value);
}

void BinaryWriter::write_f32(float value)
{
    write_floating(*out_, value);
}

void BinaryWriter::write_f64(double value)
{
    write_floating(*out_, value);
}

void BinaryWriter::write_string(const std::string & value)
{
    if (value.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error("string is too large to encode");
    }
    write_u32(static_cast<std::uint32_t>(value.size()));
    if (!value.empty()) {
        write_bytes(value.data(), value.size());
    }
}

void BinaryWriter::write_value(const schema::Value & value)
{
    std::visit(
        [this](const auto & data) {
            using T = std::decay_t<decltype(data)>;
            if constexpr (std::is_same_v<T, schema::NullValue>) {
                write_u8(static_cast<std::uint8_t>(EncodedValueKind::Null));
            } else if constexpr (std::is_same_v<T, bool>) {
                write_u8(static_cast<std::uint8_t>(EncodedValueKind::Boolean));
                write_u8(data ? 1U : 0U);
            } else if constexpr (std::is_same_v<T, std::int32_t>) {
                write_u8(static_cast<std::uint8_t>(EncodedValueKind::Integer));
                write_i32(data);
            } else if constexpr (std::is_same_v<T, std::int64_t>) {
                write_u8(static_cast<std::uint8_t>(EncodedValueKind::BigInt));
                write_i64(data);
            } else if constexpr (std::is_same_v<T, float>) {
                write_u8(static_cast<std::uint8_t>(EncodedValueKind::Float));
                write_f32(data);
            } else if constexpr (std::is_same_v<T, double>) {
                write_u8(static_cast<std::uint8_t>(EncodedValueKind::Double));
                write_f64(data);
            } else if constexpr (std::is_same_v<T, std::string>) {
                write_u8(static_cast<std::uint8_t>(EncodedValueKind::String));
                write_string(data);
            } else if constexpr (std::is_same_v<T, schema::VectorValue>) {
                if (data.size() > std::numeric_limits<std::uint32_t>::max()) {
                    throw std::runtime_error("vector is too large to encode");
                }
                write_u8(static_cast<std::uint8_t>(EncodedValueKind::Vector));
                write_u32(static_cast<std::uint32_t>(data.size()));
                for (const auto element : data) {
                    write_f64(element);
                }
            }
        },
        value.data()
    );
}

void BinaryWriter::write_bytes(const void * data, std::size_t size)
{
    out_->write(static_cast<const char *>(data), static_cast<std::streamsize>(size));
    if (!*out_) {
        throw std::runtime_error("failed to write binary data");
    }
}

BinaryReader::BinaryReader(std::istream & in) noexcept
    : in_(&in)
{
}

bool BinaryReader::try_read_u32(std::uint32_t & value)
{
    char bytes[sizeof(std::uint32_t)] {};
    in_->read(bytes, sizeof(bytes));
    if (in_->gcount() == 0 && in_->eof()) {
        return false;
    }
    if (in_->gcount() != static_cast<std::streamsize>(sizeof(bytes))) {
        return false;
    }
    std::uint32_t result = 0;
    for (std::size_t index = 0; index < sizeof(std::uint32_t); ++index) {
        result |= static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[index])) << (index * 8U);
    }
    value = result;
    return true;
}

bool BinaryReader::try_read_bytes(void * data, std::size_t size)
{
    in_->read(static_cast<char *>(data), static_cast<std::streamsize>(size));
    return in_->gcount() == static_cast<std::streamsize>(size);
}

std::uint8_t BinaryReader::read_u8()
{
    std::uint8_t value = 0;
    read_bytes(&value, sizeof(value));
    return value;
}

std::uint16_t BinaryReader::read_u16()
{
    return read_little_endian<std::uint16_t>(*in_);
}

std::uint32_t BinaryReader::read_u32()
{
    return read_little_endian<std::uint32_t>(*in_);
}

std::uint64_t BinaryReader::read_u64()
{
    return read_little_endian<std::uint64_t>(*in_);
}

std::int32_t BinaryReader::read_i32()
{
    return read_little_endian<std::int32_t>(*in_);
}

std::int64_t BinaryReader::read_i64()
{
    return read_little_endian<std::int64_t>(*in_);
}

float BinaryReader::read_f32()
{
    return read_floating<float>(*in_);
}

double BinaryReader::read_f64()
{
    return read_floating<double>(*in_);
}

std::string BinaryReader::read_string()
{
    const auto size = read_u32();
    std::string value(size, '\0');
    if (size != 0) {
        read_bytes(value.data(), value.size());
    }
    return value;
}

schema::Value BinaryReader::read_value()
{
    const auto kind = static_cast<EncodedValueKind>(read_u8());
    switch (kind) {
    case EncodedValueKind::Null:
        return schema::Value::null();
    case EncodedValueKind::Boolean:
        return schema::Value {read_u8() != 0};
    case EncodedValueKind::Integer:
        return schema::Value {read_i32()};
    case EncodedValueKind::BigInt:
        return schema::Value {read_i64()};
    case EncodedValueKind::Float:
        return schema::Value {read_f32()};
    case EncodedValueKind::Double:
        return schema::Value {read_f64()};
    case EncodedValueKind::String:
        return schema::Value {read_string()};
    case EncodedValueKind::Vector: {
        const auto count = read_u32();
        schema::VectorValue values;
        values.reserve(count);
        for (std::uint32_t index = 0; index < count; ++index) {
            values.push_back(read_f64());
        }
        return schema::Value {std::move(values)};
    }
    }

    throw std::runtime_error("invalid encoded value kind");
}

void BinaryReader::read_bytes(void * data, std::size_t size)
{
    in_->read(static_cast<char *>(data), static_cast<std::streamsize>(size));
    if (!*in_) {
        throw std::runtime_error("unexpected end of binary data");
    }
}

} // namespace litedb::core::persistence
