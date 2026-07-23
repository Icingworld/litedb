#pragma once

#ifndef _WIN32

#include "core/filesystem/backend/filesystem_backend.hpp"

namespace litedb::core::filesystem::backend
{

class PosixFileSystemBackend final : public FileSystemBackend
{
public:
    std::expected<std::unique_ptr<FileHandleBackend>, FileSystemError> open(
        const std::filesystem::path & path,
        const FileOpenOptions & options
    ) override;

    std::expected<std::vector<std::filesystem::path>, FileSystemError> list_dir(
        const std::filesystem::path & path
    ) override;

    std::expected<bool, FileSystemError> exists(const std::filesystem::path & path) override;

    std::expected<void, FileSystemError> create_dir_all(const std::filesystem::path & path) override;

    std::expected<void, FileSystemError> rename(
        const std::filesystem::path & from,
        const std::filesystem::path & to
    ) override;

    std::expected<void, FileSystemError> replace_file_atomic(
        const std::filesystem::path & from,
        const std::filesystem::path & to
    ) override;

    std::expected<void, FileSystemError> remove(const std::filesystem::path & path) override;

    std::expected<void, FileSystemError> sync_directory(const std::filesystem::path & path) override;
};

} // namespace litedb::core::filesystem::backend

#endif // _WIN32
