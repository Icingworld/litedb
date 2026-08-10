#include "core/filesystem/filesystem.hpp"

#include <string>
#include <utility>

#include "core/filesystem/backend/filesystem_backend.hpp"
#include "core/filesystem/filesystem_error.hpp"

namespace litedb::core::filesystem
{

namespace
{

error::Error invalid_state_error(
    std::string operation,
    const std::filesystem::path & path,
    const std::filesystem::path & related_path = {}
)
{
    const auto message = operation + " failed because the filesystem has no backend";
    FileSystemErrorContext context {
        std::move(operation),
        path,
        related_path,
        {},
    };
    return error::Error {FileSystemErrorCode::InvalidState, message, std::move(context)};
}

} // namespace

FileSystem::FileSystem(std::unique_ptr<backend::FileSystemBackend> backend)
    : backend_(std::move(backend))
{}

FileSystem::FileSystem(FileSystem &&) noexcept = default;

FileSystem & FileSystem::operator=(FileSystem && other) noexcept
{
    if (this != &other) {
        backend_ = std::move(other.backend_);
    }
    return *this;
}

FileSystem::~FileSystem() = default;

std::expected<FileHandle, error::Error>
FileSystem::open(const std::filesystem::path & path, const FileOpenOptions & options)
{
    if (!backend_) [[unlikely]] {
        return std::unexpected(invalid_state_error("open", path));
    }

    if (options.access == FileAccess::ReadOnly &&
        (options.create_mode == FileCreateMode::TruncateExisting ||
         options.create_mode == FileCreateMode::CreateOrTruncate)) [[unlikely]] {
        FileSystemErrorContext context {
            "open",
            path,
            std::filesystem::path(),
            std::error_code(),
        };
        return std::unexpected(
            error::Error(
                FileSystemErrorCode::InvalidArgument,
                "open failed: truncate create mode requires write access",
                std::move(context)
            )
        );
    }

    auto result = backend_->open(path, options);
    if (!result) [[unlikely]] {
        return std::unexpected(std::move(result.error()));
    }
    return FileHandle {std::move(*result)};
}

std::expected<std::vector<std::filesystem::path>, error::Error> FileSystem::list_dir(
    const std::filesystem::path & path
)
{
    if (!backend_) [[unlikely]] {
        return std::unexpected(invalid_state_error("list_dir", path));
    }
    return backend_->list_dir(path);
}

std::expected<bool, error::Error> FileSystem::exists(const std::filesystem::path & path)
{
    if (!backend_) [[unlikely]] {
        return std::unexpected(invalid_state_error("exists", path));
    }
    return backend_->exists(path);
}

std::expected<void, error::Error> FileSystem::create_dir_all(const std::filesystem::path & path)
{
    if (!backend_) [[unlikely]] {
        return std::unexpected(invalid_state_error("create_dir_all", path));
    }
    return backend_->create_dir_all(path);
}

std::expected<void, error::Error>
FileSystem::rename(const std::filesystem::path & from, const std::filesystem::path & to)
{
    if (!backend_) [[unlikely]] {
        return std::unexpected(invalid_state_error("rename", from, to));
    }
    return backend_->rename(from, to);
}

std::expected<void, error::Error> FileSystem::replace_file_atomic(
    const std::filesystem::path & from,
    const std::filesystem::path & to
)
{
    if (!backend_) [[unlikely]] {
        return std::unexpected(invalid_state_error("replace_file_atomic", from, to));
    }
    return backend_->replace_file_atomic(from, to);
}

std::expected<void, error::Error> FileSystem::remove(const std::filesystem::path & path)
{
    if (!backend_) [[unlikely]] {
        return std::unexpected(invalid_state_error("remove", path));
    }
    return backend_->remove(path);
}

std::expected<void, error::Error> FileSystem::sync_directory(const std::filesystem::path & path)
{
    if (!backend_) [[unlikely]] {
        return std::unexpected(invalid_state_error("sync_directory", path));
    }
    return backend_->sync_directory(path);
}

} // namespace litedb::core::filesystem
