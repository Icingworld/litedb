#include "core/io/buffer_byte_writer.hpp"

#include <utility>

namespace litedb::core::io
{

std::expected<void, IoError> BufferByteWriter::write_bytes(std::span<const std::byte> data)
{
    bytes_.insert(bytes_.end(), data.begin(), data.end());
    return {};
}

const std::vector<std::byte> & BufferByteWriter::bytes() const noexcept
{
    return bytes_;
}

std::vector<std::byte> BufferByteWriter::take_bytes() noexcept
{
    return std::move(bytes_);
}

} // namespace litedb::core::io
