#include "core/io/file_byte_reader.hpp"

#include <utility>

namespace litedb::core::io
{

namespace
{

IoError from_filesystem_error(filesystem::FileSystemError error)
{
    return IoError {
        .code = IoErrorCode::FileSystemError,
        .message = std::move(error.message),
    };
}

} // namespace

FileByteReader::FileByteReader(filesystem::FileHandle & file) noexcept
    : file_(&file)
    , offset_(0)
{
}

std::expected<std::size_t, IoError> FileByteReader::read_some(std::span<std::byte> data)
{
    auto result = file_->read_at(offset_, data);
    if (!result.has_value()) {
        return std::unexpected(from_filesystem_error(std::move(result.error())));
    }
    offset_ += result.value();
    return result.value();
}

std::uint64_t FileByteReader::offset() const noexcept
{
    return offset_;
}

} // namespace litedb::core::io
