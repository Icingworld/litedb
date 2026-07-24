#include "core/filesystem/filesystem.hpp"

#include <cassert>
#include <utility>

#include "core/filesystem/backend/filesystem_backend.hpp"
#include "core/filesystem/filesystem_error.hpp"

namespace litedb::core::filesystem
{

FileSystem::FileSystem(std::unique_ptr<backend::FileSystemBackend> backend)
    : backend_(std::move(backend))
{
    assert(backend_);
}

FileSystem::FileSystem(FileSystem &&) noexcept = default;

FileSystem & FileSystem::operator=(FileSystem &&) noexcept = default;

FileSystem::~FileSystem() = default;

std::expected<FileHandle, error::Error> FileSystem::open(
    const std::filesystem::path & path,
    const FileOpenOptions & options
)
{
    assert(backend_);

    if (options.access == FileAccess::ReadOnly &&
        (options.create_mode == FileCreateMode::TruncateExisting ||
         options.create_mode == FileCreateMode::CreateOrTruncate)) {

        FileSystemErrorContext context {
            "open",
            path,
            std::filesystem::path(),
            std::error_code(),
        };
        return std::unexpected(error::Error (
            FileSystemErrorCode::InvalidArgument,
            "open failed: truncate create mode requires write access",
            std::move(context)
        ));
    }

    auto result = backend_->open(path, options);
    if (!result) {
        return std::unexpected(std::move(result.error()));
    }
    return FileHandle {std::move(*result)};
}

std::expected<std::vector<std::filesystem::path>, error::Error> FileSystem::list_dir(
    const std::filesystem::path & path
)
{
    assert(backend_);
    return backend_->list_dir(path);
}

std::expected<bool, error::Error> FileSystem::exists(const std::filesystem::path & path)
{
    assert(backend_);
    return backend_->exists(path);
}

std::expected<void, error::Error> FileSystem::create_dir_all(const std::filesystem::path & path)
{
    assert(backend_);
    return backend_->create_dir_all(path);
}

std::expected<void, error::Error> FileSystem::rename(
    const std::filesystem::path & from,
    const std::filesystem::path & to
)
{
    assert(backend_);
    return backend_->rename(from, to);
}

std::expected<void, error::Error> FileSystem::replace_file_atomic(
    const std::filesystem::path & from,
    const std::filesystem::path & to
)
{
    assert(backend_);
    return backend_->replace_file_atomic(from, to);
}

std::expected<void, error::Error> FileSystem::remove(const std::filesystem::path & path)
{
    assert(backend_);
    return backend_->remove(path);
}

std::expected<void, error::Error> FileSystem::sync_directory(const std::filesystem::path & path)
{
    assert(backend_);
    return backend_->sync_directory(path);
}

} // namespace litedb::core::filesystem
