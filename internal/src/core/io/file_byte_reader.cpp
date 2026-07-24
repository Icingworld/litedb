#include "core/io/file_byte_reader.hpp"

namespace litedb::core::io
{

FileByteReader::FileByteReader(filesystem::FileHandle & file) noexcept
    : file_(&file)
    , offset_(0)
{
}

std::expected<std::size_t, IoError> FileByteReader::read_some(std::span<std::byte> data)
{
    auto result = file_->read_at(offset_, data);
    if (!result) {
        return std::unexpected(std::move(result.error()));
    }
    const auto read = *result;
    offset_ += read;
    return read;
}

std::uint64_t FileByteReader::offset() const noexcept
{
    return offset_;
}

} // namespace litedb::core::io
