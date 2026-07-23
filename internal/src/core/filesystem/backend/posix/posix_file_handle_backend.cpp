#ifndef _WIN32

#include "core/filesystem/backend/posix/posix_file_handle_backend.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <limits>
#include <string>
#include <utility>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace litedb::core::filesystem::backend
{

namespace
{

FileSystemErrorCode map_errno(int error)
{
    switch (error) {
    case ENOENT:
        return FileSystemErrorCode::NotFound;
    case EEXIST:
        return FileSystemErrorCode::AlreadyExists;
    case EACCES:
    case EPERM:
    case EBADF:
        return FileSystemErrorCode::PermissionDenied;
    case EINVAL:
        return FileSystemErrorCode::InvalidArgument;
    case ENAMETOOLONG:
        return FileSystemErrorCode::InvalidPath;
    case ENOTDIR:
        return FileSystemErrorCode::NotADirectory;
    case EISDIR:
        return FileSystemErrorCode::NotAFile;
    case ENOTEMPTY:
        return FileSystemErrorCode::DirectoryNotEmpty;
    case EROFS:
        return FileSystemErrorCode::ReadOnly;
    case ENOSPC:
        return FileSystemErrorCode::NoSpace;
    case EBUSY:
        return FileSystemErrorCode::ResourceBusy;
    default:
        return FileSystemErrorCode::IoError;
    }
}

FileSystemError make_errno_error(
    int error,
    std::string operation,
    const std::filesystem::path & path
)
{
    const std::error_code native_code(error, std::generic_category());
    return FileSystemError {
        map_errno(error),
        operation + " failed: " + std::strerror(error),
        std::move(operation),
        path,
        {},
        native_code,
    };
}

FileSystemError range_error(
    std::string operation,
    const std::filesystem::path & path
)
{
    return FileSystemError {
        FileSystemErrorCode::InvalidArgument,
        operation + " range exceeds the native file offset limit",
        std::move(operation),
        path,
    };
}

FileSystemError closed_error(
    std::string operation,
    const std::filesystem::path & path
)
{
    return FileSystemError {
        FileSystemErrorCode::InvalidArgument,
        operation + " failed because the file handle is closed",
        std::move(operation),
        path,
    };
}

std::expected<off_t, FileSystemError> checked_range(
    std::uint64_t offset,
    std::size_t size,
    std::string operation,
    const std::filesystem::path & path
)
{
    constexpr auto max_offset = static_cast<std::uint64_t>(std::numeric_limits<off_t>::max());
    if (offset > max_offset || size > max_offset - offset) {
        return std::unexpected(range_error(std::move(operation), path));
    }
    return static_cast<off_t>(offset);
}

std::expected<void, FileSystemError> write_all_at(
    int fd,
    const std::byte * data,
    std::size_t size,
    off_t offset,
    const std::filesystem::path & path
)
{
    std::size_t written_total = 0;
    while (written_total < size) {
        const auto chunk_size = std::min<std::size_t>(
            size - written_total,
            static_cast<std::size_t>(std::numeric_limits<ssize_t>::max())
        );
        const ssize_t written = ::pwrite(fd, data + written_total, chunk_size, offset + written_total);
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return std::unexpected(make_errno_error(errno, "pwrite", path));
        }
        if (written == 0) {
            return std::unexpected(FileSystemError {
                FileSystemErrorCode::IoError,
                "pwrite wrote zero bytes",
                "pwrite",
                path,
            });
        }
        written_total += static_cast<std::size_t>(written);
    }
    return {};
}

} // namespace

PosixFileHandleBackend::PosixFileHandleBackend(int fd, std::filesystem::path path)
    : fd_(fd)
    , path_(std::move(path))
{
}

PosixFileHandleBackend::~PosixFileHandleBackend()
{
    static_cast<void>(close());
}

std::expected<void, FileSystemError> PosixFileHandleBackend::close()
{
    std::scoped_lock lock {mutex_};
    if (fd_ < 0) {
        return {};
    }
    const int fd = fd_;
    fd_ = -1;
    if (::close(fd) != 0) {
        return std::unexpected(make_errno_error(errno, "close", path_));
    }
    return {};
}

std::expected<std::size_t, FileSystemError> PosixFileHandleBackend::read_at(
    std::uint64_t offset,
    std::span<std::byte> buffer
)
{
    std::scoped_lock lock {mutex_};
    if (fd_ < 0) {
        return std::unexpected(closed_error("pread", path_));
    }

    const auto native_offset = checked_range(offset, buffer.size(), "pread", path_);
    if (!native_offset) {
        return std::unexpected(std::move(native_offset.error()));
    }

    std::size_t read_total = 0;
    while (read_total < buffer.size()) {
        const auto chunk_size = std::min<std::size_t>(
            buffer.size() - read_total,
            static_cast<std::size_t>(std::numeric_limits<ssize_t>::max())
        );
        const ssize_t read = ::pread(fd_, buffer.data() + read_total, chunk_size, *native_offset + read_total);
        if (read < 0) {
            if (errno == EINTR) {
                continue;
            }
            return std::unexpected(make_errno_error(errno, "pread", path_));
        }
        if (read == 0) {
            break;
        }
        read_total += static_cast<std::size_t>(read);
    }
    return read_total;
}

std::expected<void, FileSystemError> PosixFileHandleBackend::write_at(
    std::uint64_t offset,
    std::span<const std::byte> data
)
{
    std::scoped_lock lock {mutex_};
    if (fd_ < 0) {
        return std::unexpected(closed_error("pwrite", path_));
    }

    const auto native_offset = checked_range(offset, data.size(), "pwrite", path_);
    if (!native_offset) {
        return std::unexpected(std::move(native_offset.error()));
    }
    return write_all_at(fd_, data.data(), data.size(), *native_offset, path_);
}

std::expected<void, FileSystemError> PosixFileHandleBackend::append(std::span<const std::byte> data)
{
    std::scoped_lock lock {mutex_};
    if (fd_ < 0) {
        return std::unexpected(closed_error("append", path_));
    }

    const off_t offset = ::lseek(fd_, 0, SEEK_END);
    if (offset < 0) {
        return std::unexpected(make_errno_error(errno, "lseek", path_));
    }
    const auto native_offset = checked_range(
        static_cast<std::uint64_t>(offset),
        data.size(),
        "append",
        path_
    );
    if (!native_offset) {
        return std::unexpected(std::move(native_offset.error()));
    }
    return write_all_at(fd_, data.data(), data.size(), *native_offset, path_);
}

std::expected<std::uint64_t, FileSystemError> PosixFileHandleBackend::size()
{
    std::scoped_lock lock {mutex_};
    if (fd_ < 0) {
        return std::unexpected(closed_error("fstat", path_));
    }

    struct stat stat_buffer {};
    if (::fstat(fd_, &stat_buffer) != 0) {
        return std::unexpected(make_errno_error(errno, "fstat", path_));
    }
    return static_cast<std::uint64_t>(stat_buffer.st_size);
}

std::expected<void, FileSystemError> PosixFileHandleBackend::truncate(std::uint64_t size)
{
    std::scoped_lock lock {mutex_};
    if (fd_ < 0) {
        return std::unexpected(closed_error("ftruncate", path_));
    }

    const auto native_size = checked_range(size, 0, "ftruncate", path_);
    if (!native_size) {
        return std::unexpected(std::move(native_size.error()));
    }
    if (::ftruncate(fd_, *native_size) != 0) {
        return std::unexpected(make_errno_error(errno, "ftruncate", path_));
    }
    return {};
}

std::expected<void, FileSystemError> PosixFileHandleBackend::sync_data()
{
    std::scoped_lock lock {mutex_};
    if (fd_ < 0) {
        return std::unexpected(closed_error("fdatasync", path_));
    }

#if defined(__APPLE__)
    if (::fcntl(fd_, F_FULLFSYNC) == 0) {
        return {};
    }
    if (errno != EINVAL && errno != ENOTSUP) {
        return std::unexpected(make_errno_error(errno, "fcntl(F_FULLFSYNC)", path_));
    }
#endif
    while (::fdatasync(fd_) != 0) {
        if (errno == EINTR) {
            continue;
        }
        return std::unexpected(make_errno_error(errno, "fdatasync", path_));
    }
    return {};
}

std::expected<void, FileSystemError> PosixFileHandleBackend::sync_all()
{
    std::scoped_lock lock {mutex_};
    if (fd_ < 0) {
        return std::unexpected(closed_error("fsync", path_));
    }

    while (::fsync(fd_) != 0) {
        if (errno == EINTR) {
            continue;
        }
        return std::unexpected(make_errno_error(errno, "fsync", path_));
    }
    return {};
}

} // namespace litedb::core::filesystem::backend

#endif // _WIN32
