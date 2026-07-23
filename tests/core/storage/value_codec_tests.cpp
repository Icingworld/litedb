#include "core/io/buffer_byte_reader.hpp"
#include "core/io/buffer_byte_writer.hpp"
#include "core/storage/value_codec.hpp"

#include <array>
#include <exception>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace
{

using namespace litedb::core;

void require(bool condition, const char * message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void require_ok(std::expected<void, io::IoError> result, const char * message)
{
    if (!result) {
        throw std::runtime_error(message);
    }
}

template <typename T>
const T & get(const common::Value & value)
{
    return std::get<T>(value.data());
}

void test_value_format_and_roundtrip()
{
    io::BufferByteWriter bytes {256};
    io::BinaryWriter writer {bytes};
    require_ok(storage::write_value(writer, common::Value::null()), "write null failed");
    require_ok(storage::write_value(writer, common::Value {true}), "write bool failed");
    require_ok(storage::write_value(writer, common::Value {std::int32_t {-2}}), "write int failed");
    require_ok(storage::write_value(writer, common::Value {std::int64_t {7}}), "write bigint failed");
    require_ok(storage::write_value(writer, common::Value {1.0F}), "write float failed");
    require_ok(storage::write_value(writer, common::Value {1.0}), "write double failed");
    require_ok(storage::write_value(writer, common::Value {std::string {"hi"}}), "write string failed");
    require_ok(storage::write_value(
        writer,
        common::Value {common::VectorValue {1.0, 2.0}}
    ), "write vector failed");

    const std::vector<std::byte> expected {
        std::byte {0x00},
        std::byte {0x01}, std::byte {0x01},
        std::byte {0x02}, std::byte {0xfe}, std::byte {0xff}, std::byte {0xff}, std::byte {0xff},
        std::byte {0x03}, std::byte {0x07}, std::byte {0x00}, std::byte {0x00}, std::byte {0x00},
        std::byte {0x00}, std::byte {0x00}, std::byte {0x00}, std::byte {0x00},
        std::byte {0x04}, std::byte {0x00}, std::byte {0x00}, std::byte {0x80}, std::byte {0x3f},
        std::byte {0x05}, std::byte {0x00}, std::byte {0x00}, std::byte {0x00}, std::byte {0x00},
        std::byte {0x00}, std::byte {0x00}, std::byte {0xf0}, std::byte {0x3f},
        std::byte {0x06}, std::byte {0x02}, std::byte {0x00}, std::byte {0x00}, std::byte {0x00},
        std::byte {0x68}, std::byte {0x69},
        std::byte {0x07}, std::byte {0x02}, std::byte {0x00}, std::byte {0x00}, std::byte {0x00},
        std::byte {0x00}, std::byte {0x00}, std::byte {0x00}, std::byte {0x00},
        std::byte {0x00}, std::byte {0x00}, std::byte {0xf0}, std::byte {0x3f},
        std::byte {0x00}, std::byte {0x00}, std::byte {0x00}, std::byte {0x00},
        std::byte {0x00}, std::byte {0x00}, std::byte {0x00}, std::byte {0x40},
    };
    require(bytes.bytes() == expected, "value wire format changed");

    io::BufferByteReader source {bytes.bytes()};
    io::BinaryReader reader {
        source,
        {.max_total_bytes = bytes.bytes().size(), .max_string_bytes = 64},
    };
    const storage::ValueDecodeLimits limits {.max_vector_elements = 16};
    auto null_value = storage::read_value(reader, limits);
    auto bool_value = storage::read_value(reader, limits);
    auto int_value = storage::read_value(reader, limits);
    auto bigint_value = storage::read_value(reader, limits);
    auto float_value = storage::read_value(reader, limits);
    auto double_value = storage::read_value(reader, limits);
    auto string_value = storage::read_value(reader, limits);
    auto vector_value = storage::read_value(reader, limits);
    require(null_value && null_value->is_null(), "null roundtrip mismatch");
    require(bool_value && get<bool>(*bool_value), "bool roundtrip mismatch");
    require(int_value && get<std::int32_t>(*int_value) == -2, "int roundtrip mismatch");
    require(bigint_value && get<std::int64_t>(*bigint_value) == 7, "bigint roundtrip mismatch");
    require(float_value && get<float>(*float_value) == 1.0F, "float roundtrip mismatch");
    require(double_value && get<double>(*double_value) == 1.0, "double roundtrip mismatch");
    require(string_value && get<std::string>(*string_value) == "hi", "string roundtrip mismatch");
    require(vector_value && get<common::VectorValue>(*vector_value) == common::VectorValue({1.0, 2.0}),
            "vector roundtrip mismatch");
}

void test_invalid_values_are_rejected()
{
    const std::array invalid_bool {std::byte {0x01}, std::byte {0x02}};
    io::BufferByteReader bool_source {invalid_bool};
    io::BinaryReader bool_reader {
        bool_source,
        {.max_total_bytes = invalid_bool.size(), .max_string_bytes = 16},
    };
    auto boolean = storage::read_value(bool_reader, {.max_vector_elements = 16});
    require(!boolean && boolean.error().is(io::IoErrorCode::InvalidData),
            "non-canonical boolean accepted");

    const std::array huge_vector {
        std::byte {0x07},
        std::byte {0xff}, std::byte {0xff}, std::byte {0xff}, std::byte {0xff},
    };
    io::BufferByteReader vector_source {huge_vector};
    io::BinaryReader vector_reader {
        vector_source,
        {.max_total_bytes = huge_vector.size(), .max_string_bytes = 16},
    };
    auto vector = storage::read_value(vector_reader, {.max_vector_elements = 512});
    require(!vector && vector.error().is(io::IoErrorCode::ValueTooLarge),
            "oversized vector count accepted");
}

} // namespace

int main()
{
    try {
        test_value_format_and_roundtrip();
        test_invalid_values_are_rejected();
    } catch (const std::exception &) {
        return 1;
    }
    return 0;
}
