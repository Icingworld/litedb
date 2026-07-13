#include "core/io/file_byte_writer.hpp"

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

FileByteWriter::FileByteWriter(filesystem::FileHandle & file, std::uint64_t offset) noexcept
    : file_(&file)
    , offset_(offset)
{
}

std::expected<void, IoError> FileByteWriter::write_bytes(std::span<const std::byte> data)
{
    auto written = file_->write_at(offset_, data);
    if (!written.has_value()) {
        return std::unexpected(from_filesystem_error(std::move(written.error())));
    }
    offset_ += data.size();
    return {};
}

std::uint64_t FileByteWriter::offset() const noexcept
{
    return offset_;
}

FileByteAppender::FileByteAppender(filesystem::FileHandle & file) noexcept
    : file_(&file)
{
}

std::expected<void, IoError> FileByteAppender::write_bytes(std::span<const std::byte> data)
{
    auto written = file_->append(data);
    if (!written.has_value()) {
        return std::unexpected(from_filesystem_error(std::move(written.error())));
    }
    return {};
}

} // namespace litedb::core::io
