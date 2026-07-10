#include "core/io/binary_io.hpp"
#include "core/io/buffer_byte_reader.hpp"
#include "core/io/buffer_byte_writer.hpp"

#include <cstdint>
#include <exception>
#include <stdexcept>
#include <string>
#include <variant>

namespace
{

using namespace litedb::core;

void require(bool condition, const char * message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <typename T>
T require_value(std::expected<T, io::IoError> result, const char * message)
{
    if (!result.has_value()) {
        throw std::runtime_error(message);
    }
    return std::move(result.value());
}

void require_ok(std::expected<void, io::IoError> result, const char * message)
{
    if (!result.has_value()) {
        throw std::runtime_error(message);
    }
}

template <typename T>
const T & get_value(const schema::Value & value)
{
    return std::get<T>(value.data());
}

void test_binary_roundtrip()
{
    io::BufferByteWriter bytes;
    io::BinaryWriter writer {bytes};
    require_ok(writer.write_u32(42), "write u32 failed");
    require_ok(writer.write_string("hello"), "write string failed");
    require_ok(writer.write_value(schema::Value {std::int64_t {7}}), "write bigint failed");
    require_ok(writer.write_value(schema::Value {schema::VectorValue {1.0, 2.0, 3.0}}), "write vector failed");

    io::BufferByteReader input {bytes.bytes()};
    io::BinaryReader reader {input};
    require(require_value(reader.read_u32(), "read u32 failed") == 42, "u32 roundtrip mismatch");
    require(require_value(reader.read_string(), "read string failed") == "hello", "string roundtrip mismatch");
    const auto bigint = require_value(reader.read_value(), "read bigint failed");
    require(get_value<std::int64_t>(bigint) == 7, "bigint value mismatch");
    const auto vector_value = require_value(reader.read_value(), "read vector failed");
    const auto vector = get_value<schema::VectorValue>(vector_value);
    require(vector.size() == 3, "vector size mismatch");
    require(vector[1] == 2.0, "vector value mismatch");
}

} // namespace

int main()
{
    try {
        test_binary_roundtrip();
    } catch (const std::exception & exception) {
        (void) exception;
        return 1;
    }

    return 0;
}
