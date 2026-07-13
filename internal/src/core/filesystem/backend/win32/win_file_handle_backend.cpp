#ifdef _WIN32

#include "core/filesystem/backend/win32/win_file_handle_backend.hpp"

#include <algorithm>
#include <cassert>
#include <limits>
#include <string>
#include <utility>

namespace litedb::core::filesystem::backend
{

namespace
{

FileSystemErrorCode map_win32_error(DWORD error)
{
    switch (error) {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
        return FileSystemErrorCode::NotFound;
    case ERROR_FILE_EXISTS:
    case ERROR_ALREADY_EXISTS:
        return FileSystemErrorCode::AlreadyExists;
    case ERROR_ACCESS_DENIED:
        return FileSystemErrorCode::PermissionDenied;
    case ERROR_INVALID_NAME:
    case ERROR_BAD_PATHNAME:
    case ERROR_FILENAME_EXCED_RANGE:
        return FileSystemErrorCode::InvalidPath;
    case ERROR_INVALID_PARAMETER:
    case ERROR_NEGATIVE_SEEK:
        return FileSystemErrorCode::InvalidArgument;
    case ERROR_DIR_NOT_EMPTY:
        return FileSystemErrorCode::DirectoryNotEmpty;
    case ERROR_WRITE_PROTECT:
        return FileSystemErrorCode::ReadOnly;
    case ERROR_DISK_FULL:
    case ERROR_HANDLE_DISK_FULL:
        return FileSystemErrorCode::NoSpace;
    case ERROR_SHARING_VIOLATION:
    case ERROR_LOCK_VIOLATION:
        return FileSystemErrorCode::ResourceBusy;
    default:
        return FileSystemErrorCode::IoError;
    }
}

std::string win32_error_message(DWORD error)
{
    LPWSTR buffer = nullptr;
    const DWORD size = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&buffer),
        0,
        nullptr
    );

    std::wstring wide_message;
    if (size != 0 && buffer != nullptr) {
        wide_message.assign(buffer, size);
        LocalFree(buffer);
    }

    if (wide_message.empty()) {
        return "Win32 error " + std::to_string(error);
    }

    const int utf8_size = WideCharToMultiByte(
        CP_UTF8,
        0,
        wide_message.data(),
        static_cast<int>(wide_message.size()),
        nullptr,
        0,
        nullptr,
        nullptr
    );
    if (utf8_size <= 0) {
        return "Win32 error " + std::to_string(error);
    }

    std::string message(static_cast<std::size_t>(utf8_size), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        wide_message.data(),
        static_cast<int>(wide_message.size()),
        message.data(),
        utf8_size,
        nullptr,
        nullptr
    );
    while (!message.empty() && (message.back() == '\r' || message.back() == '\n')) {
        message.pop_back();
    }
    return message;
}

FileSystemError make_win32_error(DWORD error, std::string operation)
{
    return FileSystemError {
        map_win32_error(error),
        std::move(operation) + " failed: " + win32_error_message(error),
    };
}

std::expected<void, FileSystemError> write_all_locked(
    HANDLE handle,
    const std::byte * data,
    std::size_t size
)
{
    std::size_t written_total = 0;
    while (written_total < size) {
        const auto chunk_size = std::min<std::size_t>(
            size - written_total,
            static_cast<std::size_t>(std::numeric_limits<DWORD>::max())
        );

        DWORD written = 0;
        if (!WriteFile(
                handle,
                data + written_total,
                static_cast<DWORD>(chunk_size),
                &written,
                nullptr
            )) {
            return std::unexpected(make_win32_error(GetLastError(), "WriteFile"));
        }
        if (written == 0) {
            return std::unexpected(FileSystemError {FileSystemErrorCode::IoError, "WriteFile wrote zero bytes"});
        }
        written_total += written;
    }
    return {};
}

} // namespace

Win32FileHandleBackend::Win32FileHandleBackend(HANDLE handle)
    : handle_(handle)
{
    assert(handle_ != INVALID_HANDLE_VALUE);
}

Win32FileHandleBackend::~Win32FileHandleBackend()
{
    static_cast<void>(close());
}

std::expected<void, FileSystemError> Win32FileHandleBackend::close()
{
    std::scoped_lock lock {mutex_};
    if (handle_ == INVALID_HANDLE_VALUE) {
        return {};
    }

    const HANDLE handle = handle_;
    handle_ = INVALID_HANDLE_VALUE;
    if (!CloseHandle(handle)) {
        return std::unexpected(make_win32_error(GetLastError(), "CloseHandle"));
    }
    return {};
}

std::expected<void, FileSystemError> Win32FileHandleBackend::seek_locked(std::uint64_t offset)
{
    LARGE_INTEGER distance {};
    if (offset > static_cast<std::uint64_t>(std::numeric_limits<LONGLONG>::max())) {
        return std::unexpected(FileSystemError {FileSystemErrorCode::InvalidArgument, "file offset is too large"});
    }
    distance.QuadPart = static_cast<LONGLONG>(offset);
    if (!SetFilePointerEx(handle_, distance, nullptr, FILE_BEGIN)) {
        return std::unexpected(make_win32_error(GetLastError(), "SetFilePointerEx"));
    }
    return {};
}

std::expected<std::size_t, FileSystemError> Win32FileHandleBackend::read_at(
    std::uint64_t offset,
    std::span<std::byte> buffer
)
{
    std::scoped_lock lock {mutex_};
    if (handle_ == INVALID_HANDLE_VALUE) {
        return std::unexpected(FileSystemError {FileSystemErrorCode::InvalidArgument, "file handle is closed"});
    }

    if (const auto seek = seek_locked(offset); !seek) {
        return std::unexpected(std::move(seek.error()));
    }

    std::size_t read_total = 0;
    while (read_total < buffer.size()) {
        const auto chunk_size = std::min<std::size_t>(
            buffer.size() - read_total,
            static_cast<std::size_t>(std::numeric_limits<DWORD>::max())
        );

        DWORD read = 0;
        if (!ReadFile(
                handle_,
                buffer.data() + read_total,
                static_cast<DWORD>(chunk_size),
                &read,
                nullptr
            )) {
            return std::unexpected(make_win32_error(GetLastError(), "ReadFile"));
        }
        if (read == 0) {
            break;
        }
        read_total += read;
    }
    return read_total;
}

std::expected<void, FileSystemError> Win32FileHandleBackend::write_at(
    std::uint64_t offset,
    std::span<const std::byte> data
)
{
    std::scoped_lock lock {mutex_};
    if (handle_ == INVALID_HANDLE_VALUE) {
        return std::unexpected(FileSystemError {FileSystemErrorCode::InvalidArgument, "file handle is closed"});
    }

    if (const auto seek = seek_locked(offset); !seek) {
        return seek;
    }
    return write_all_locked(handle_, data.data(), data.size());
}

std::expected<void, FileSystemError> Win32FileHandleBackend::append(std::span<const std::byte> data)
{
    std::scoped_lock lock {mutex_};
    if (handle_ == INVALID_HANDLE_VALUE) {
        return std::unexpected(FileSystemError {FileSystemErrorCode::InvalidArgument, "file handle is closed"});
    }

    LARGE_INTEGER distance {};
    if (!SetFilePointerEx(handle_, distance, nullptr, FILE_END)) {
        return std::unexpected(make_win32_error(GetLastError(), "SetFilePointerEx"));
    }
    return write_all_locked(handle_, data.data(), data.size());
}

std::expected<std::uint64_t, FileSystemError> Win32FileHandleBackend::size()
{
    std::scoped_lock lock {mutex_};
    if (handle_ == INVALID_HANDLE_VALUE) {
        return std::unexpected(FileSystemError {FileSystemErrorCode::InvalidArgument, "file handle is closed"});
    }

    LARGE_INTEGER file_size {};
    if (!GetFileSizeEx(handle_, &file_size)) {
        return std::unexpected(make_win32_error(GetLastError(), "GetFileSizeEx"));
    }
    return static_cast<std::uint64_t>(file_size.QuadPart);
}

std::expected<void, FileSystemError> Win32FileHandleBackend::truncate(std::uint64_t size)
{
    std::scoped_lock lock {mutex_};
    if (handle_ == INVALID_HANDLE_VALUE) {
        return std::unexpected(FileSystemError {FileSystemErrorCode::InvalidArgument, "file handle is closed"});
    }

    if (const auto seek = seek_locked(size); !seek) {
        return seek;
    }
    if (!SetEndOfFile(handle_)) {
        return std::unexpected(make_win32_error(GetLastError(), "SetEndOfFile"));
    }
    return {};
}

std::expected<void, FileSystemError> Win32FileHandleBackend::sync_data()
{
    return sync_all();
}

std::expected<void, FileSystemError> Win32FileHandleBackend::sync_all()
{
    std::scoped_lock lock {mutex_};
    if (handle_ == INVALID_HANDLE_VALUE) {
        return std::unexpected(FileSystemError {FileSystemErrorCode::InvalidArgument, "file handle is closed"});
    }

    if (!FlushFileBuffers(handle_)) {
        return std::unexpected(make_win32_error(GetLastError(), "FlushFileBuffers"));
    }
    return {};
}

} // namespace litedb::core::filesystem::backend

#endif // _WIN32
