#ifdef _WIN32

#include "core/filesystem/backend/win32/win_filesystem_backend.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <memory>
#include <system_error>
#include <utility>

#include "core/filesystem/backend/win32/win_file_handle_backend.hpp"

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
    return FileSystemErrorCode::IoError;
}

std::string display_path(const std::filesystem::path & path)
{
    try {
        return path.string();
    } catch (...) {
        return "<unprintable path>";
    }
}

FileSystemError make_error(
    const std::error_code & error,
    std::string operation,
    const std::filesystem::path & path,
    const std::filesystem::path & related_path = {}
)
{
    auto message = operation + " '" + display_path(path) + "'";
    if (!related_path.empty()) {
        message += " -> '" + display_path(related_path) + "'";
    }
    message += " failed: " + error.message();
    return FileSystemError {
        map_error_code(error),
        std::move(message),
        std::move(operation),
        path,
        related_path,
        error,
    };
}

FileSystemError make_win32_error(
    DWORD error,
    std::string operation,
    const std::filesystem::path & path,
    const std::filesystem::path & related_path = {}
)
{
    return make_error(
        std::error_code(static_cast<int>(error), std::system_category()),
        std::move(operation),
        path,
        related_path
    );
}

DWORD to_desired_access(FileAccess access)
{
    switch (access) {
    case FileAccess::ReadOnly:
        return GENERIC_READ;
    case FileAccess::WriteOnly:
        return GENERIC_WRITE;
    case FileAccess::ReadWrite:
        return GENERIC_READ | GENERIC_WRITE;
    }
    return 0;
}

DWORD to_creation_disposition(FileCreateMode mode)
{
    switch (mode) {
    case FileCreateMode::OpenExisting:
        return OPEN_EXISTING;
    case FileCreateMode::OpenOrCreate:
        return OPEN_ALWAYS;
    case FileCreateMode::CreateNew:
        return CREATE_NEW;
    case FileCreateMode::TruncateExisting:
        return TRUNCATE_EXISTING;
    case FileCreateMode::CreateOrTruncate:
        return CREATE_ALWAYS;
    }
    return OPEN_EXISTING;
}

std::expected<std::wstring, FileSystemError> to_native_path(const std::filesystem::path & path)
{
    try {
        return path.native();
    } catch (const std::exception & error) {
        return std::unexpected(FileSystemError {
            FileSystemErrorCode::InvalidPath,
            "native path conversion failed for '" + display_path(path) + "': " + error.what(),
            "path.native",
            path,
        });
    }
}

} // namespace

std::unique_ptr<FileSystemBackend> create_platform_filesystem_backend()
{
    return std::make_unique<Win32FileSystemBackend>();
}

std::expected<std::unique_ptr<FileHandleBackend>, FileSystemError> Win32FileSystemBackend::open(
    const std::filesystem::path & path,
    const FileOpenOptions & options
)
{
    auto native_path = to_native_path(path);
    if (!native_path) {
        return std::unexpected(std::move(native_path.error()));
    }

    const HANDLE handle = CreateFileW(
        native_path->c_str(),
        to_desired_access(options.access),
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        to_creation_disposition(options.create_mode),
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    if (handle == INVALID_HANDLE_VALUE) {
        return std::unexpected(make_win32_error(GetLastError(), "CreateFileW", path));
    }

    std::unique_ptr<FileHandleBackend> backend = std::make_unique<Win32FileHandleBackend>(handle, path);
    return backend;
}

std::expected<std::vector<std::filesystem::path>, FileSystemError> Win32FileSystemBackend::list_dir(
    const std::filesystem::path & path
)
{
    std::error_code error;
    if (!std::filesystem::is_directory(path, error)) {
        if (error) {
            return std::unexpected(make_error(error, "is_directory", path));
        }
        return std::unexpected(FileSystemError {
            FileSystemErrorCode::NotADirectory,
            "path is not a directory: '" + display_path(path) + "'",
            "is_directory",
            path,
        });
    }

    std::vector<std::filesystem::path> entries;
    for (std::filesystem::directory_iterator it {path, error}, end; it != end; it.increment(error)) {
        if (error) {
            return std::unexpected(make_error(error, "directory_iterator", path));
        }
        entries.push_back(it->path().filename());
    }
    return entries;
}

std::expected<bool, FileSystemError> Win32FileSystemBackend::exists(const std::filesystem::path & path)
{
    std::error_code error;
    const bool result = std::filesystem::exists(path, error);
    if (error) {
        return std::unexpected(make_error(error, "exists", path));
    }
    return result;
}

std::expected<void, FileSystemError> Win32FileSystemBackend::create_dir_all(
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

std::expected<void, FileSystemError> Win32FileSystemBackend::rename(
    const std::filesystem::path & from,
    const std::filesystem::path & to
)
{
    auto native_from = to_native_path(from);
    if (!native_from) {
        return std::unexpected(std::move(native_from.error()));
    }
    auto native_to = to_native_path(to);
    if (!native_to) {
        return std::unexpected(std::move(native_to.error()));
    }

    if (!MoveFileExW(native_from->c_str(), native_to->c_str(), MOVEFILE_WRITE_THROUGH)) {
        return std::unexpected(make_win32_error(GetLastError(), "MoveFileExW", from, to));
    }
    return {};
}

std::expected<void, FileSystemError> Win32FileSystemBackend::replace_file_atomic(
    const std::filesystem::path & from,
    const std::filesystem::path & to
)
{
    auto native_from = to_native_path(from);
    if (!native_from) {
        return std::unexpected(std::move(native_from.error()));
    }
    auto native_to = to_native_path(to);
    if (!native_to) {
        return std::unexpected(std::move(native_to.error()));
    }

    if (!MoveFileExW(
            native_from->c_str(),
            native_to->c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH
        )) {
        return std::unexpected(make_win32_error(
            GetLastError(),
            "MoveFileExW",
            from,
            to
        ));
    }
    return {};
}

std::expected<void, FileSystemError> Win32FileSystemBackend::remove(const std::filesystem::path & path)
{
    std::error_code error;
    std::filesystem::remove(path, error);
    if (error) {
        return std::unexpected(make_error(error, "remove", path));
    }
    return {};
}

std::expected<void, FileSystemError> Win32FileSystemBackend::sync_directory(
    const std::filesystem::path & path
)
{
    auto native_path = to_native_path(path);
    if (!native_path) {
        return std::unexpected(std::move(native_path.error()));
    }

    const HANDLE handle = CreateFileW(
        native_path->c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        nullptr
    );
    if (handle == INVALID_HANDLE_VALUE) {
        return std::unexpected(make_win32_error(GetLastError(), "CreateFileW", path));
    }

    const bool ok = FlushFileBuffers(handle);
    const DWORD error = GetLastError();
    CloseHandle(handle);
    if (!ok) {
        if (error == ERROR_INVALID_FUNCTION || error == ERROR_ACCESS_DENIED) {
            return std::unexpected(FileSystemError {
                FileSystemErrorCode::Unsupported,
                "directory sync is not supported by this Windows filesystem",
                "FlushFileBuffers",
                path,
            });
        }
        return std::unexpected(make_win32_error(error, "FlushFileBuffers", path));
    }
    return {};
}

} // namespace litedb::core::filesystem::backend

#endif // _WIN32
