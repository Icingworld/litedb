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

error::Error make_error(
    FileSystemErrorCode code,
    std::string message,
    std::string operation,
    const std::filesystem::path & path,
    std::error_code native_code = {}
)
{
    FileSystemErrorContext context {
        std::move(operation),
        path,
        {},
        std::move(native_code),
    };
    return error::Error {code, message, std::move(context)};
}

error::Error
make_win32_error(DWORD error, std::string operation, const std::filesystem::path & path)
{
    const std::error_code native_code(static_cast<int>(error), std::system_category());
    return make_error(
        map_win32_error(error),
        operation + " failed: " + win32_error_message(error),
        std::move(operation),
        path,
        native_code
    );
}

error::Error range_error(std::string operation, const std::filesystem::path & path)
{
    const auto message = operation + " range exceeds the native file offset limit";
    return make_error(FileSystemErrorCode::InvalidArgument, message, std::move(operation), path);
}

error::Error closed_error(std::string operation, const std::filesystem::path & path)
{
    return make_error(
        FileSystemErrorCode::InvalidArgument,
        "file handle is closed",
        std::move(operation),
        path
    );
}

std::expected<void, error::Error> write_all_locked(
    HANDLE handle,
    const std::byte * data,
    std::size_t size,
    const std::filesystem::path & path
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
            return std::unexpected(make_win32_error(GetLastError(), "WriteFile", path));
        }
        if (written == 0) {
            return std::unexpected(make_error(
                FileSystemErrorCode::IoError,
                "WriteFile wrote zero bytes",
                "WriteFile",
                path
            ));
        }
        written_total += written;
    }
    return {};
}

} // namespace

Win32FileHandleBackend::Win32FileHandleBackend(HANDLE handle, std::filesystem::path path)
    : handle_(handle)
    , path_(std::move(path))
{
    assert(handle_ != INVALID_HANDLE_VALUE);
}

Win32FileHandleBackend::~Win32FileHandleBackend()
{
    static_cast<void>(close());
}

std::expected<void, error::Error> Win32FileHandleBackend::close()
{
    std::scoped_lock lock {mutex_};
    if (handle_ == INVALID_HANDLE_VALUE) {
        return {};
    }

    const HANDLE handle = handle_;
    handle_ = INVALID_HANDLE_VALUE;
    if (!CloseHandle(handle)) {
        return std::unexpected(make_win32_error(GetLastError(), "CloseHandle", path_));
    }
    return {};
}

std::expected<void, error::Error>
Win32FileHandleBackend::seek_locked(std::uint64_t offset, std::size_t size, const char * operation)
{
    LARGE_INTEGER distance {};
    constexpr auto max_offset = static_cast<std::uint64_t>(std::numeric_limits<LONGLONG>::max());
    if (offset > max_offset || size > max_offset - offset) {
        return std::unexpected(range_error(operation, path_));
    }
    distance.QuadPart = static_cast<LONGLONG>(offset);
    if (!SetFilePointerEx(handle_, distance, nullptr, FILE_BEGIN)) {
        return std::unexpected(make_win32_error(GetLastError(), "SetFilePointerEx", path_));
    }
    return {};
}

std::expected<std::size_t, error::Error>
Win32FileHandleBackend::read_at(std::uint64_t offset, std::span<std::byte> buffer)
{
    std::scoped_lock lock {mutex_};
    if (handle_ == INVALID_HANDLE_VALUE) {
        return std::unexpected(closed_error("ReadFile", path_));
    }

    if (auto seek = seek_locked(offset, buffer.size(), "ReadFile"); !seek) {
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
            return std::unexpected(make_win32_error(GetLastError(), "ReadFile", path_));
        }
        if (read == 0) {
            break;
        }
        read_total += read;
    }
    return read_total;
}

std::expected<void, error::Error>
Win32FileHandleBackend::write_at(std::uint64_t offset, std::span<const std::byte> data)
{
    std::scoped_lock lock {mutex_};
    if (handle_ == INVALID_HANDLE_VALUE) {
        return std::unexpected(closed_error("WriteFile", path_));
    }

    if (auto seek = seek_locked(offset, data.size(), "WriteFile"); !seek) {
        return std::move(seek);
    }
    return write_all_locked(handle_, data.data(), data.size(), path_);
}

std::expected<void, error::Error> Win32FileHandleBackend::append(std::span<const std::byte> data)
{
    std::scoped_lock lock {mutex_};
    if (handle_ == INVALID_HANDLE_VALUE) {
        return std::unexpected(closed_error("append", path_));
    }

    LARGE_INTEGER distance {};
    LARGE_INTEGER end {};
    if (!SetFilePointerEx(handle_, distance, &end, FILE_END)) {
        return std::unexpected(make_win32_error(GetLastError(), "SetFilePointerEx", path_));
    }
    constexpr auto max_offset = static_cast<std::uint64_t>(std::numeric_limits<LONGLONG>::max());
    if (end.QuadPart < 0 || data.size() > max_offset - static_cast<std::uint64_t>(end.QuadPart)) {
        return std::unexpected(range_error("append", path_));
    }
    return write_all_locked(handle_, data.data(), data.size(), path_);
}

std::expected<std::uint64_t, error::Error> Win32FileHandleBackend::size()
{
    std::scoped_lock lock {mutex_};
    if (handle_ == INVALID_HANDLE_VALUE) {
        return std::unexpected(closed_error("GetFileSizeEx", path_));
    }

    LARGE_INTEGER file_size {};
    if (!GetFileSizeEx(handle_, &file_size)) {
        return std::unexpected(make_win32_error(GetLastError(), "GetFileSizeEx", path_));
    }
    if (file_size.QuadPart < 0) {
        return std::unexpected(range_error("GetFileSizeEx", path_));
    }
    return static_cast<std::uint64_t>(file_size.QuadPart);
}

std::expected<void, error::Error> Win32FileHandleBackend::truncate(std::uint64_t size)
{
    std::scoped_lock lock {mutex_};
    if (handle_ == INVALID_HANDLE_VALUE) {
        return std::unexpected(closed_error("SetEndOfFile", path_));
    }

    if (auto seek = seek_locked(size, 0, "SetEndOfFile"); !seek) {
        return std::move(seek);
    }
    if (!SetEndOfFile(handle_)) {
        return std::unexpected(make_win32_error(GetLastError(), "SetEndOfFile", path_));
    }
    return {};
}

std::expected<void, error::Error> Win32FileHandleBackend::sync_data()
{
    return sync_all();
}

std::expected<void, error::Error> Win32FileHandleBackend::sync_all()
{
    std::scoped_lock lock {mutex_};
    if (handle_ == INVALID_HANDLE_VALUE) {
        return std::unexpected(closed_error("FlushFileBuffers", path_));
    }

    if (!FlushFileBuffers(handle_)) {
        return std::unexpected(make_win32_error(GetLastError(), "FlushFileBuffers", path_));
    }
    return {};
}

} // namespace litedb::core::filesystem::backend

#endif // _WIN32
