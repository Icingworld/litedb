#include "core/io/buffer_byte_reader.hpp"

#include <algorithm>
#include <cstring>

namespace litedb::core::io
{

BufferByteReader::BufferByteReader(std::span<const std::byte> data) noexcept
    : data_(data)
    , offset_(0)
{}

std::expected<std::size_t, IoError> BufferByteReader::read_some(std::span<std::byte> data)
{
    const auto remaining = static_cast<std::uint64_t>(data_.size()) - offset_;
    const auto count = std::min<std::uint64_t>(remaining, data.size());
    if (count != 0) {
        std::memcpy(data.data(), data_.data() + offset_, static_cast<std::size_t>(count));
    }
    offset_ += count;
    return static_cast<std::size_t>(count);
}

} // namespace litedb::core::io
