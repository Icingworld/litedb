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

FileSystemError make_errno_error(int error, std::string operation)
{
    return FileSystemError {
        map_errno(error),
        std::move(operation) + " failed: " + std::strerror(error),
    };
}

std::expected<void, FileSystemError> write_all_at(int fd, const std::byte * data, std::size_t size, off_t offset)
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
            return std::unexpected(make_errno_error(errno, "pwrite"));
        }
        if (written == 0) {
            return std::unexpected(FileSystemError {FileSystemErrorCode::IoError, "pwrite wrote zero bytes"});
        }
        written_total += static_cast<std::size_t>(written);
    }
    return {};
}

std::expected<off_t, FileSystemError> checked_offset(std::uint64_t offset)
{
    if (offset > static_cast<std::uint64_t>(std::numeric_limits<off_t>::max())) {
        return std::unexpected(FileSystemError {FileSystemErrorCode::InvalidArgument, "file offset is too large"});
    }
    return static_cast<off_t>(offset);
}

} // namespace

PosixFileHandleBackend::PosixFileHandleBackend(int fd)
    : fd_(fd)
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
    while (::close(fd) != 0) {
        if (errno == EINTR) {
            continue;
        }
        return std::unexpected(make_errno_error(errno, "close"));
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
        return std::unexpected(FileSystemError {FileSystemErrorCode::InvalidArgument, "file handle is closed"});
    }

    const auto native_offset = checked_offset(offset);
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
            return std::unexpected(make_errno_error(errno, "pread"));
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
        return std::unexpected(FileSystemError {FileSystemErrorCode::InvalidArgument, "file handle is closed"});
    }

    const auto native_offset = checked_offset(offset);
    if (!native_offset) {
        return std::unexpected(std::move(native_offset.error()));
    }
    return write_all_at(fd_, data.data(), data.size(), *native_offset);
}

std::expected<void, FileSystemError> PosixFileHandleBackend::append(std::span<const std::byte> data)
{
    std::scoped_lock lock {mutex_};
    if (fd_ < 0) {
        return std::unexpected(FileSystemError {FileSystemErrorCode::InvalidArgument, "file handle is closed"});
    }

    const off_t offset = ::lseek(fd_, 0, SEEK_END);
    if (offset < 0) {
        return std::unexpected(make_errno_error(errno, "lseek"));
    }
    return write_all_at(fd_, data.data(), data.size(), offset);
}

std::expected<std::uint64_t, FileSystemError> PosixFileHandleBackend::size()
{
    std::scoped_lock lock {mutex_};
    if (fd_ < 0) {
        return std::unexpected(FileSystemError {FileSystemErrorCode::InvalidArgument, "file handle is closed"});
    }

    struct stat stat_buffer {};
    if (::fstat(fd_, &stat_buffer) != 0) {
        return std::unexpected(make_errno_error(errno, "fstat"));
    }
    return static_cast<std::uint64_t>(stat_buffer.st_size);
}

std::expected<void, FileSystemError> PosixFileHandleBackend::truncate(std::uint64_t size)
{
    std::scoped_lock lock {mutex_};
    if (fd_ < 0) {
        return std::unexpected(FileSystemError {FileSystemErrorCode::InvalidArgument, "file handle is closed"});
    }

    const auto native_size = checked_offset(size);
    if (!native_size) {
        return std::unexpected(std::move(native_size.error()));
    }
    if (::ftruncate(fd_, *native_size) != 0) {
        return std::unexpected(make_errno_error(errno, "ftruncate"));
    }
    return {};
}

std::expected<void, FileSystemError> PosixFileHandleBackend::sync_data()
{
    std::scoped_lock lock {mutex_};
    if (fd_ < 0) {
        return std::unexpected(FileSystemError {FileSystemErrorCode::InvalidArgument, "file handle is closed"});
    }

#if defined(__APPLE__)
    if (::fcntl(fd_, F_FULLFSYNC) == 0) {
        return {};
    }
    if (errno != EINVAL && errno != ENOTSUP) {
        return std::unexpected(make_errno_error(errno, "fcntl(F_FULLFSYNC)"));
    }
#endif
    while (::fdatasync(fd_) != 0) {
        if (errno == EINTR) {
            continue;
        }
        return std::unexpected(make_errno_error(errno, "fdatasync"));
    }
    return {};
}

std::expected<void, FileSystemError> PosixFileHandleBackend::sync_all()
{
    std::scoped_lock lock {mutex_};
    if (fd_ < 0) {
        return std::unexpected(FileSystemError {FileSystemErrorCode::InvalidArgument, "file handle is closed"});
    }

    while (::fsync(fd_) != 0) {
        if (errno == EINTR) {
            continue;
        }
        return std::unexpected(make_errno_error(errno, "fsync"));
    }
    return {};
}

} // namespace litedb::core::filesystem::backend

#endif // _WIN32
