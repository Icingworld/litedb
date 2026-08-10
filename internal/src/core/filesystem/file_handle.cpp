#include "core/filesystem/file_handle.hpp"

#include <string>
#include <utility>

#include "core/filesystem/backend/file_handle_backend.hpp"

namespace litedb::core::filesystem
{

namespace
{

error::Error invalid_state_error(std::string operation)
{
    const auto message = operation + " failed because the file handle has no backend";
    FileSystemErrorContext context {
        std::move(operation),
        {},
        {},
        {},
    };
    return error::Error {FileSystemErrorCode::InvalidState, message, std::move(context)};
}

} // namespace

FileHandle::FileHandle(std::unique_ptr<backend::FileHandleBackend> backend)
    : backend_(std::move(backend))
{}

FileHandle::FileHandle(FileHandle &&) noexcept = default;

FileHandle & FileHandle::operator=(FileHandle && other) noexcept
{
    if (this != &other) {
        backend_ = std::move(other.backend_);
    }
    return *this;
}

FileHandle::~FileHandle() = default;

std::expected<void, error::Error> FileHandle::close()
{
    if (!backend_) [[unlikely]] {
        return std::unexpected(invalid_state_error("close"));
    }
    return backend_->close();
}

std::expected<std::size_t, error::Error>
FileHandle::read_at(std::uint64_t offset, std::span<std::byte> buffer)
{
    if (!backend_) [[unlikely]] {
        return std::unexpected(invalid_state_error("read_at"));
    }
    return backend_->read_at(offset, buffer);
}

std::expected<void, error::Error>
FileHandle::write_at(std::uint64_t offset, std::span<const std::byte> data)
{
    if (!backend_) [[unlikely]] {
        return std::unexpected(invalid_state_error("write_at"));
    }
    return backend_->write_at(offset, data);
}

std::expected<void, error::Error> FileHandle::append(std::span<const std::byte> data)
{
    if (!backend_) [[unlikely]] {
        return std::unexpected(invalid_state_error("append"));
    }
    return backend_->append(data);
}

std::expected<std::uint64_t, error::Error> FileHandle::size()
{
    if (!backend_) [[unlikely]] {
        return std::unexpected(invalid_state_error("size"));
    }
    return backend_->size();
}

std::expected<void, error::Error> FileHandle::truncate(std::uint64_t size)
{
    if (!backend_) [[unlikely]] {
        return std::unexpected(invalid_state_error("truncate"));
    }
    return backend_->truncate(size);
}

std::expected<void, error::Error> FileHandle::sync_data()
{
    if (!backend_) [[unlikely]] {
        return std::unexpected(invalid_state_error("sync_data"));
    }
    return backend_->sync_data();
}

std::expected<void, error::Error> FileHandle::sync_all()
{
    if (!backend_) [[unlikely]] {
        return std::unexpected(invalid_state_error("sync_all"));
    }
    return backend_->sync_all();
}

} // namespace litedb::core::filesystem
