#include "core/io/binary_io.hpp"
#include "core/io/buffer_byte_reader.hpp"
#include "core/io/buffer_byte_writer.hpp"
#include "core/io/file_byte_reader.hpp"
#include "core/io/file_byte_writer.hpp"
#include "core/filesystem/backend/file_handle_backend.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <exception>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

using namespace litedb::core;

constexpr io::BinaryDecodeLimits limits(std::size_t size, std::uint32_t max_string = 1024)
{
    return {
        .max_total_bytes = size,
        .max_string_bytes = max_string,
    };
}

void require(bool condition, const char * message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <typename T>
T require_value(std::expected<T, io::IoError> result, const char * message)
{
    if (!result) {
        throw std::runtime_error(message);
    }
    return std::move(*result);
}

void require_ok(std::expected<void, io::IoError> result, const char * message)
{
    if (!result) {
        throw std::runtime_error(message);
    }
}

class CountingWriter final : public io::ByteWriter
{
public:
    std::expected<void, io::IoError> write_bytes(std::span<const std::byte> data) override
    {
        ++calls;
        bytes.insert(bytes.end(), data.begin(), data.end());
        return {};
    }

    std::size_t calls {0};
    std::vector<std::byte> bytes;
};

class ChunkedReader final : public io::ByteReader
{
public:
    explicit ChunkedReader(std::span<const std::byte> bytes)
        : bytes_(bytes)
    {
    }

    std::expected<std::size_t, io::IoError> read_some(std::span<std::byte> output) override
    {
        if (offset_ == bytes_.size()) {
            return 0;
        }
        const auto count = std::min<std::size_t>({output.size(), 1, bytes_.size() - offset_});
        std::memcpy(output.data(), bytes_.data() + offset_, count);
        offset_ += count;
        return count;
    }

private:
    std::span<const std::byte> bytes_;
    std::size_t offset_ {0};
};

struct FileState
{
    std::vector<std::byte> bytes;
    bool fail_writes {false};
    bool closed {false};
};

class TestFileBackend final : public filesystem::backend::FileHandleBackend
{
public:
    explicit TestFileBackend(std::shared_ptr<FileState> state)
        : state_(std::move(state))
    {
    }

    std::expected<void, error::Error> close() override
    {
        state_->closed = true;
        return {};
    }

    std::expected<std::size_t, error::Error> read_at(
        std::uint64_t offset,
        std::span<std::byte> output
    ) override
    {
        if (state_->closed) {
            return std::unexpected(make_error(
                filesystem::FileSystemErrorCode::InvalidArgument,
                "read_at"
            ));
        }
        if (offset >= state_->bytes.size()) {
            return 0;
        }
        const auto count = std::min(
            output.size(),
            state_->bytes.size() - static_cast<std::size_t>(offset)
        );
        std::memcpy(output.data(), state_->bytes.data() + offset, count);
        return count;
    }

    std::expected<void, error::Error> write_at(
        std::uint64_t offset,
        std::span<const std::byte> input
    ) override
    {
        if (state_->fail_writes) {
            return std::unexpected(make_error(
                filesystem::FileSystemErrorCode::NoSpace,
                "write_at"
            ));
        }
        const auto end = static_cast<std::size_t>(offset) + input.size();
        state_->bytes.resize(std::max(state_->bytes.size(), end));
        std::memcpy(state_->bytes.data() + offset, input.data(), input.size());
        return {};
    }

    std::expected<void, error::Error> append(std::span<const std::byte> input) override
    {
        if (state_->fail_writes) {
            return std::unexpected(make_error(
                filesystem::FileSystemErrorCode::NoSpace,
                "append"
            ));
        }
        state_->bytes.insert(state_->bytes.end(), input.begin(), input.end());
        return {};
    }

    std::expected<std::uint64_t, error::Error> size() override
    {
        return state_->bytes.size();
    }

    std::expected<void, error::Error> truncate(std::uint64_t size) override
    {
        state_->bytes.resize(static_cast<std::size_t>(size));
        return {};
    }

    std::expected<void, error::Error> sync_data() override { return {}; }
    std::expected<void, error::Error> sync_all() override { return {}; }

private:
    static error::Error make_error(
        filesystem::FileSystemErrorCode code,
        std::string operation
    )
    {
        filesystem::FileSystemErrorContext context {
            .operation = std::move(operation),
            .path = "test.data",
            .related_path = {},
            .native_code = {},
        };
        return error::Error {code, "test filesystem error", std::move(context)};
    }

    std::shared_ptr<FileState> state_;
};

void test_exact_primitive_encoding()
{
    io::BufferByteWriter bytes {128};
    io::LittleEndianBinaryWriter writer {bytes};
    require_ok(writer.write_u16(0x1234), "write u16 failed");
    require_ok(writer.write_u32(0x89abcdef), "write u32 failed");
    require_ok(writer.write_u64(0x0123456789abcdefULL), "write u64 failed");
    require_ok(writer.write_i32(std::numeric_limits<std::int32_t>::min()), "write i32 failed");
    require_ok(writer.write_i64(-2), "write i64 failed");
    require_ok(writer.write_f32(1.0F), "write f32 failed");
    require_ok(writer.write_f64(1.0), "write f64 failed");

    const std::vector<std::byte> expected {
        std::byte {0x34}, std::byte {0x12},
        std::byte {0xef}, std::byte {0xcd}, std::byte {0xab}, std::byte {0x89},
        std::byte {0xef}, std::byte {0xcd}, std::byte {0xab}, std::byte {0x89},
        std::byte {0x67}, std::byte {0x45}, std::byte {0x23}, std::byte {0x01},
        std::byte {0x00}, std::byte {0x00}, std::byte {0x00}, std::byte {0x80},
        std::byte {0xfe}, std::byte {0xff}, std::byte {0xff}, std::byte {0xff},
        std::byte {0xff}, std::byte {0xff}, std::byte {0xff}, std::byte {0xff},
        std::byte {0x00}, std::byte {0x00}, std::byte {0x80}, std::byte {0x3f},
        std::byte {0x00}, std::byte {0x00}, std::byte {0x00}, std::byte {0x00},
        std::byte {0x00}, std::byte {0x00}, std::byte {0xf0}, std::byte {0x3f},
    };
    require(bytes.bytes() == expected, "primitive little-endian bytes mismatch");

    io::BufferByteReader input {bytes.bytes()};
    io::LittleEndianBinaryReader reader {input, limits(bytes.bytes().size())};
    require(require_value(reader.read_u16(), "read u16 failed") == 0x1234, "u16 mismatch");
    require(require_value(reader.read_u32(), "read u32 failed") == 0x89abcdef, "u32 mismatch");
    require(require_value(reader.read_u64(), "read u64 failed") == 0x0123456789abcdefULL, "u64 mismatch");
    require(require_value(reader.read_i32(), "read i32 failed") == std::numeric_limits<std::int32_t>::min(), "i32 mismatch");
    require(require_value(reader.read_i64(), "read i64 failed") == -2, "i64 mismatch");
    require(require_value(reader.read_f32(), "read f32 failed") == 1.0F, "f32 mismatch");
    require(require_value(reader.read_f64(), "read f64 failed") == 1.0, "f64 mismatch");
    require(reader.remaining_bytes() == 0, "reader budget mismatch");
}

void test_big_endian_primitive_encoding()
{
    io::BufferByteWriter bytes {128};
    io::BigEndianBinaryWriter writer {bytes};
    require_ok(writer.write_u16(0x1234), "write u16 failed");
    require_ok(writer.write_u32(0x89abcdef), "write u32 failed");
    require_ok(writer.write_u64(0x0123456789abcdefULL), "write u64 failed");
    require_ok(writer.write_i32(std::numeric_limits<std::int32_t>::min()), "write i32 failed");
    require_ok(writer.write_i64(-2), "write i64 failed");
    require_ok(writer.write_f32(1.0F), "write f32 failed");
    require_ok(writer.write_f64(1.0), "write f64 failed");

    const std::vector<std::byte> expected {
        std::byte {0x12}, std::byte {0x34},
        std::byte {0x89}, std::byte {0xab}, std::byte {0xcd}, std::byte {0xef},
        std::byte {0x01}, std::byte {0x23}, std::byte {0x45}, std::byte {0x67},
        std::byte {0x89}, std::byte {0xab}, std::byte {0xcd}, std::byte {0xef},
        std::byte {0x80}, std::byte {0x00}, std::byte {0x00}, std::byte {0x00},
        std::byte {0xff}, std::byte {0xff}, std::byte {0xff}, std::byte {0xff},
        std::byte {0xff}, std::byte {0xff}, std::byte {0xff}, std::byte {0xfe},
        std::byte {0x3f}, std::byte {0x80}, std::byte {0x00}, std::byte {0x00},
        std::byte {0x3f}, std::byte {0xf0}, std::byte {0x00}, std::byte {0x00},
        std::byte {0x00}, std::byte {0x00}, std::byte {0x00}, std::byte {0x00},
    };
    require(bytes.bytes() == expected, "primitive big-endian bytes mismatch");

    io::BufferByteReader input {bytes.bytes()};
    io::BigEndianBinaryReader reader {input, limits(bytes.bytes().size())};
    require(require_value(reader.read_u16(), "read u16 failed") == 0x1234, "u16 mismatch");
    require(require_value(reader.read_u32(), "read u32 failed") == 0x89abcdef, "u32 mismatch");
    require(require_value(reader.read_u64(), "read u64 failed") == 0x0123456789abcdefULL, "u64 mismatch");
    require(require_value(reader.read_i32(), "read i32 failed") == std::numeric_limits<std::int32_t>::min(), "i32 mismatch");
    require(require_value(reader.read_i64(), "read i64 failed") == -2, "i64 mismatch");
    require(require_value(reader.read_f32(), "read f32 failed") == 1.0F, "f32 mismatch");
    require(require_value(reader.read_f64(), "read f64 failed") == 1.0, "f64 mismatch");
    require(reader.remaining_bytes() == 0, "reader budget mismatch");
}

void test_big_endian_string_round_trip()
{
    io::BufferByteWriter bytes {64};
    io::BigEndianBinaryWriter writer {bytes};
    require_ok(writer.write_string("hello"), "write string failed");

    const std::vector<std::byte> expected {
        std::byte {0x00}, std::byte {0x00}, std::byte {0x00}, std::byte {0x05},
        std::byte {0x68}, std::byte {0x65}, std::byte {0x6c}, std::byte {0x6c}, std::byte {0x6f},
    };
    require(bytes.bytes() == expected, "big-endian string bytes mismatch");

    io::BufferByteReader input {bytes.bytes()};
    io::BigEndianBinaryReader reader {input, limits(bytes.bytes().size())};
    require(require_value(reader.read_string(), "read string failed") == "hello", "string mismatch");
    require(reader.remaining_bytes() == 0, "reader budget mismatch");
}

void test_single_call_per_primitive()
{
    CountingWriter bytes;
    io::LittleEndianBinaryWriter writer {bytes};
    require_ok(writer.write_u16(1), "write u16 failed");
    require_ok(writer.write_u32(2), "write u32 failed");
    require_ok(writer.write_u64(3), "write u64 failed");
    require_ok(writer.write_f32(4.0F), "write f32 failed");
    require_ok(writer.write_f64(5.0), "write f64 failed");
    require(bytes.calls == 5, "each primitive must use one byte-writer call");

    ChunkedReader input {bytes.bytes};
    io::LittleEndianBinaryReader reader {input, limits(bytes.bytes.size())};
    require_value(reader.read_u16(), "chunked u16 read failed");
    require_value(reader.read_u32(), "chunked u32 read failed");
    require_value(reader.read_u64(), "chunked u64 read failed");
    require_value(reader.read_f32(), "chunked f32 read failed");
    require_value(reader.read_f64(), "chunked f64 read failed");
}

void test_string_and_resource_limits()
{
    io::BufferByteWriter bytes {16};
    io::LittleEndianBinaryWriter writer {bytes};
    require_ok(writer.write_string("hello"), "write string failed");
    const auto overflow = writer.write_string("0123456789");
    require(!overflow && overflow.error().is(io::IoErrorCode::ValueTooLarge),
            "bounded writer must reject overflow");

    io::BufferByteReader input {bytes.bytes()};
    io::LittleEndianBinaryReader reader {input, limits(bytes.bytes().size(), 5)};
    require(require_value(reader.read_string(), "read string failed") == "hello", "string mismatch");

    const std::array huge_length {
        std::byte {0xff}, std::byte {0xff}, std::byte {0xff}, std::byte {0xff},
    };
    io::BufferByteReader huge_input {huge_length};
    io::LittleEndianBinaryReader huge_reader {huge_input, limits(huge_length.size(), 1024)};
    auto huge = huge_reader.read_string();
    require(!huge && huge.error().is(io::IoErrorCode::ValueTooLarge),
            "huge declared string must be rejected before allocation");
}

void test_truncated_data()
{
    const std::array truncated {
        std::byte {0x01}, std::byte {0x02}, std::byte {0x03}, std::byte {0x04},
    };
    io::BufferByteReader input {truncated};
    io::LittleEndianBinaryReader reader {input, limits(8)};
    auto value = reader.read_u64();
    require(!value && value.error().is(io::IoErrorCode::UnexpectedEof),
            "truncated primitive must report unexpected eof");
}

void test_file_adapters_preserve_offsets_and_errors()
{
    auto state = std::make_shared<FileState>();
    filesystem::FileHandle file {std::make_unique<TestFileBackend>(state)};
    io::FileByteWriter writer {file, 2};
    const std::array payload {std::byte {0xaa}, std::byte {0xbb}};
    require_ok(writer.write_bytes(payload), "file adapter write failed");
    require(writer.offset() == 4 && state->bytes.size() == 4, "file writer offset mismatch");

    state->fail_writes = true;
    auto failed = writer.write_bytes(payload);
    require(!failed && failed.error().is(filesystem::FileSystemErrorCode::NoSpace),
            "filesystem error code was not preserved");
    const auto * context = failed.error().context<filesystem::FileSystemErrorContext>();
    require(context != nullptr && context->path == "test.data",
            "filesystem error context was not preserved");
    require(writer.offset() == 4, "failed write changed the logical offset");

    state->fail_writes = false;
    io::FileByteAppender appender {file};
    require_ok(appender.write_bytes(payload), "file append failed");
    require(state->bytes.size() == 6, "file append size mismatch");

    io::FileByteReader reader {file};
    std::array<std::byte, 6> output {};
    require_ok(reader.read_exact(output), "file adapter read failed");
    require(reader.offset() == output.size(), "file reader offset mismatch");
    require_ok(file.close(), "file close failed");
    std::array<std::byte, 1> byte {};
    auto closed = reader.read_some(byte);
    require(!closed && closed.error().is(filesystem::FileSystemErrorCode::InvalidArgument),
            "closed handle error was not preserved");
}

} // namespace

int main()
{
    try {
        test_exact_primitive_encoding();
        test_big_endian_primitive_encoding();
        test_big_endian_string_round_trip();
        test_single_call_per_primitive();
        test_string_and_resource_limits();
        test_truncated_data();
        test_file_adapters_preserve_offsets_and_errors();
    } catch (const std::exception &) {
        return 1;
    }
    return 0;
}
