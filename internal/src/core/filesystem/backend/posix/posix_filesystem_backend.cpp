#ifndef _WIN32

#include "core/filesystem/backend/posix/posix_filesystem_backend.hpp"

#include <cerrno>
#include <cstring>
#include <memory>
#include <string>
#include <system_error>
#include <utility>

#include <fcntl.h>
#if defined(__linux__)
#include <linux/fs.h>
#include <sys/syscall.h>
#endif
#include <unistd.h>

#include "core/filesystem/backend/posix/posix_file_handle_backend.hpp"

namespace litedb::core::filesystem::backend
{

namespace
{

FileSystemErrorCode map_error_code(const std::error_code & error)
{
    if (error == std::errc::no_such_file_or_directory) {
        return FileSystemErrorCode::NotFound;
    }
    if (error == std::errc::file_exists) {
        return FileSystemErrorCode::AlreadyExists;
    }
    if (error == std::errc::permission_denied) {
        return FileSystemErrorCode::PermissionDenied;
    }
    if (error == std::errc::invalid_argument) {
        return FileSystemErrorCode::InvalidArgument;
    }
    if (error == std::errc::filename_too_long) {
        return FileSystemErrorCode::InvalidPath;
    }
    if (error == std::errc::not_a_directory) {
        return FileSystemErrorCode::NotADirectory;
    }
    if (error == std::errc::is_a_directory) {
        return FileSystemErrorCode::NotAFile;
    }
    if (error == std::errc::directory_not_empty) {
        return FileSystemErrorCode::DirectoryNotEmpty;
    }
    if (error == std::errc::read_only_file_system) {
        return FileSystemErrorCode::ReadOnly;
    }
    if (error == std::errc::no_space_on_device) {
        return FileSystemErrorCode::NoSpace;
    }
    if (error == std::errc::device_or_resource_busy) {
        return FileSystemErrorCode::ResourceBusy;
    }
    if (error == std::errc::function_not_supported || error == std::errc::operation_not_supported) {
        return FileSystemErrorCode::Unsupported;
    }
    return FileSystemErrorCode::IoError;
}

error::Error make_error(
    const std::error_code & error,
    std::string operation,
    const std::filesystem::path & path,
    const std::filesystem::path & related_path = {}
)
{
    auto message = operation + " failed: " + error.message();
    FileSystemErrorContext context {
        std::move(operation),
        path,
        related_path,
        error,
    };
    return error::Error(map_error_code(error), std::move(message), std::move(context));
}

error::Error make_errno_error(
    int error,
    std::string operation,
    const std::filesystem::path & path,
    const std::filesystem::path & related_path = {}
)
{
    return make_error(
        std::error_code(error, std::generic_category()),
        std::move(operation),
        path,
        related_path
    );
}

#if !defined(__linux__) || !defined(SYS_renameat2)
error::Error unsupported_error(
    std::string operation,
    const std::filesystem::path & path,
    const std::filesystem::path & related_path
)
{
    const auto message = operation + " failed: atomic no-replace rename is not supported";
    FileSystemErrorContext context {
        std::move(operation),
        path,
        related_path,
        {},
    };
    return error::Error {FileSystemErrorCode::Unsupported, message, std::move(context)};
}
#endif

int to_access_flags(FileAccess access)
{
    switch (access) {
    case FileAccess::ReadOnly:
        return O_RDONLY;
    case FileAccess::WriteOnly:
        return O_WRONLY;
    case FileAccess::ReadWrite:
        return O_RDWR;
    }
    return O_RDONLY;
}

int to_create_flags(FileCreateMode mode)
{
    switch (mode) {
    case FileCreateMode::OpenExisting:
        return 0;
    case FileCreateMode::OpenOrCreate:
        return O_CREAT;
    case FileCreateMode::CreateNew:
        return O_CREAT | O_EXCL;
    case FileCreateMode::TruncateExisting:
        return O_TRUNC;
    case FileCreateMode::CreateOrTruncate:
        return O_CREAT | O_TRUNC;
    }
    return 0;
}

} // namespace

std::unique_ptr<FileSystemBackend> create_platform_filesystem_backend()
{
    return std::make_unique<PosixFileSystemBackend>();
}

std::expected<std::unique_ptr<FileHandleBackend>, error::Error>
PosixFileSystemBackend::open(const std::filesystem::path & path, const FileOpenOptions & options)
{
    const int flags =
        to_access_flags(options.access) | to_create_flags(options.create_mode) | O_CLOEXEC;
    int fd = -1;
    do {
        fd = ::open(path.c_str(), flags, 0666);
    } while (fd < 0 && errno == EINTR);

    if (fd < 0) {
        return std::unexpected(make_errno_error(errno, "open", path));
    }

    std::unique_ptr<FileHandleBackend> backend = std::make_unique<PosixFileHandleBackend>(fd, path);
    return backend;
}

std::expected<std::vector<std::filesystem::path>, error::Error> PosixFileSystemBackend::list_dir(
    const std::filesystem::path & path
)
{
    std::error_code error;
    if (!std::filesystem::is_directory(path, error)) {
        if (error) {
            return std::unexpected(make_error(error, "is_directory", path));
        }
        FileSystemErrorContext context {
            "is_directory",
            path,
            {},
            {},
        };
        return std::unexpected(
            error::Error {
                FileSystemErrorCode::NotADirectory,
                "path is not a directory",
                std::move(context),
            }
        );
    }

    std::vector<std::filesystem::path> entries;
    for (std::filesystem::directory_iterator it {path, error}, end; it != end;
         it.increment(error)) {
        if (error) {
            return std::unexpected(make_error(error, "directory_iterator", path));
        }
        entries.push_back(it->path().filename());
    }
    return entries;
}

std::expected<bool, error::Error> PosixFileSystemBackend::exists(const std::filesystem::path & path)
{
    std::error_code error;
    const bool result = std::filesystem::exists(path, error);
    if (error) {
        return std::unexpected(make_error(error, "exists", path));
    }
    return result;
}

std::expected<void, error::Error> PosixFileSystemBackend::create_dir_all(
    const std::filesystem::path & path
)
{
    std::error_code error;
    std::filesystem::create_directories(path, error);
    if (error) {
        return std::unexpected(make_error(error, "create_directories", path));
    }
    return {};
}

std::expected<void, error::Error>
PosixFileSystemBackend::rename(const std::filesystem::path & from, const std::filesystem::path & to)
{
#if defined(__linux__) && defined(SYS_renameat2)
    if (::syscall(SYS_renameat2, AT_FDCWD, from.c_str(), AT_FDCWD, to.c_str(), RENAME_NOREPLACE) !=
        0) {
        return std::unexpected(make_errno_error(errno, "renameat2", from, to));
    }
    return {};
#else
    return std::unexpected(unsupported_error("rename", from, to));
#endif
}

std::expected<void, error::Error> PosixFileSystemBackend::replace_file_atomic(
    const std::filesystem::path & from,
    const std::filesystem::path & to
)
{
    std::error_code error;
    std::filesystem::rename(from, to, error);
    if (error) {
        return std::unexpected(make_error(error, "replace_file_atomic", from, to));
    }
    return {};
}

std::expected<void, error::Error> PosixFileSystemBackend::remove(const std::filesystem::path & path)
{
    std::error_code error;
    std::filesystem::remove(path, error);
    if (error) {
        return std::unexpected(make_error(error, "remove", path));
    }
    return {};
}

std::expected<void, error::Error> PosixFileSystemBackend::sync_directory(
    const std::filesystem::path & path
)
{
    int fd = -1;
    do {
        fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_DIRECTORY);
    } while (fd < 0 && errno == EINTR);

    if (fd < 0) {
        return std::unexpected(make_errno_error(errno, "open directory", path));
    }

    while (::fsync(fd) != 0) {
        if (errno == EINTR) {
            continue;
        }
        const int error = errno;
        ::close(fd);
        return std::unexpected(make_errno_error(error, "fsync directory", path));
    }
    ::close(fd);
    return {};
}

} // namespace litedb::core::filesystem::backend

#endif // _WIN32
