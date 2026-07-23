#pragma once

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <cstdint>
#include <expected>
#include <filesystem>
#include <mutex>
#include <span>

#include "core/filesystem/backend/file_handle_backend.hpp"
#include "core/filesystem/filesystem_error.hpp"

namespace litedb::core::filesystem::backend
{

class Win32FileHandleBackend final : public FileHandleBackend
{
public:
    Win32FileHandleBackend(HANDLE handle, std::filesystem::path path);

    Win32FileHandleBackend(const Win32FileHandleBackend &) = delete;

    Win32FileHandleBackend & operator=(const Win32FileHandleBackend &) = delete;

    ~Win32FileHandleBackend() override;

public:
    std::expected<void, FileSystemError> close() override;

    std::expected<std::size_t, FileSystemError> read_at(
        std::uint64_t offset,
        std::span<std::byte> buffer
    ) override;

    std::expected<void, FileSystemError> write_at(
        std::uint64_t offset,
        std::span<const std::byte> data
    ) override;

    std::expected<void, FileSystemError> append(std::span<const std::byte> data) override;

    std::expected<std::uint64_t, FileSystemError> size() override;

    std::expected<void, FileSystemError> truncate(std::uint64_t size) override;

    std::expected<void, FileSystemError> sync_data() override;

    std::expected<void, FileSystemError> sync_all() override;

private:
    std::expected<void, FileSystemError> seek_locked(
        std::uint64_t offset,
        std::size_t size,
        const char * operation
    );

    HANDLE handle_ {INVALID_HANDLE_VALUE};
    std::filesystem::path path_;
    std::mutex mutex_;
};

} // namespace litedb::core::filesystem::backend

#endif // _WIN32
