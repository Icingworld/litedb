#include "core/io/buffer_byte_writer.hpp"

#include <utility>

#include "core/io/io_helper.hpp"

namespace litedb::core::io
{

BufferByteWriter::BufferByteWriter(std::size_t max_bytes) noexcept
    : max_bytes_(max_bytes)
{}

std::expected<void, IoError> BufferByteWriter::write_bytes(std::span<const std::byte> data)
{
    if (bytes_.size() > max_bytes_ || data.size() > max_bytes_ - bytes_.size()) [[unlikely]] {
        return std::unexpected(
            make_io_error(IoErrorCode::ValueTooLarge, "buffer exceeds the configured size limit")
        );
    }
    bytes_.insert(bytes_.end(), data.begin(), data.end());
    return {};
}

const std::vector<std::byte> & BufferByteWriter::bytes() const noexcept
{
    return bytes_;
}

std::vector<std::byte> BufferByteWriter::take_bytes() noexcept
{
    return std::exchange(bytes_, {});
}

} // namespace litedb::core::io
