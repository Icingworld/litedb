#include "core/io/file_byte_reader.hpp"

#include <utility>

namespace litedb::core::io
{

namespace
{

IOError from_filesystem_error(error::Error source)
{
    const auto source_code = source.encode_code();
    return IOError {
        IOErrorCode::FileSystemError,
        source.message(),
        IOErrorContext {.source_code = source_code},
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
    const auto count = *result;
    offset_ += count;
    return count;
}

std::uint64_t FileByteReader::offset() const noexcept
{
    return offset_;
}

} // namespace litedb::core::io
