#include "core/filesystem/file_handle.hpp"

#include <cassert>
#include <utility>

#include "core/filesystem/backend/file_handle_backend.hpp"

namespace litedb::core::filesystem
{

FileHandle::FileHandle(std::unique_ptr<backend::FileHandleBackend> backend)
    : backend_(std::move(backend))
{
    assert(backend_);
}

FileHandle::FileHandle(FileHandle &&) noexcept = default;

FileHandle & FileHandle::operator=(FileHandle &&) noexcept = default;

FileHandle::~FileHandle() = default;

std::expected<std::size_t, FileSystemError> FileHandle::read_at(
    std::uint64_t offset,
    std::span<std::byte> buffer
)
{
    assert(backend_);
    return backend_->read_at(offset, buffer);
}

std::expected<void, FileSystemError> FileHandle::write_at(
    std::uint64_t offset,
    std::span<const std::byte> data
)
{
    assert(backend_);
    return backend_->write_at(offset, data);
}

std::expected<void, FileSystemError> FileHandle::append(std::span<const std::byte> data)
{
    assert(backend_);
    return backend_->append(data);
}

std::expected<std::uint64_t, FileSystemError> FileHandle::size()
{
    assert(backend_);
    return backend_->size();
}

std::expected<void, FileSystemError> FileHandle::truncate(std::uint64_t size)
{
    assert(backend_);
    return backend_->truncate(size);
}

std::expected<void, FileSystemError> FileHandle::sync_data()
{
    assert(backend_);
    return backend_->sync_data();
}

std::expected<void, FileSystemError> FileHandle::sync_all()
{
    assert(backend_);
    return backend_->sync_all();
}

} // namespace litedb::core::filesystem
