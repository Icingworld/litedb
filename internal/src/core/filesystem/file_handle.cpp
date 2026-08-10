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

std::expected<void, error::Error> FileHandle::close()
{
    return backend_->close();
}

std::expected<std::size_t, error::Error>
FileHandle::read_at(std::uint64_t offset, std::span<std::byte> buffer)
{
    return backend_->read_at(offset, buffer);
}

std::expected<void, error::Error>
FileHandle::write_at(std::uint64_t offset, std::span<const std::byte> data)
{
    return backend_->write_at(offset, data);
}

std::expected<void, error::Error> FileHandle::append(std::span<const std::byte> data)
{
    return backend_->append(data);
}

std::expected<std::uint64_t, error::Error> FileHandle::size()
{
    return backend_->size();
}

std::expected<void, error::Error> FileHandle::truncate(std::uint64_t size)
{
    return backend_->truncate(size);
}

std::expected<void, error::Error> FileHandle::sync_data()
{
    return backend_->sync_data();
}

std::expected<void, error::Error> FileHandle::sync_all()
{
    return backend_->sync_all();
}

} // namespace litedb::core::filesystem
