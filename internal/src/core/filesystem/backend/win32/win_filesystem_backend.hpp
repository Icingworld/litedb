#pragma once

#ifdef _WIN32

#include "core/filesystem/backend/filesystem_backend.hpp"

namespace litedb::core::filesystem::backend
{

class Win32FileSystemBackend final : public FileSystemBackend
{
public:
    std::expected<std::unique_ptr<FileHandleBackend>, error::Error> open(
        const std::filesystem::path & path,
        const FileOpenOptions & options
    ) override;

    std::expected<std::vector<std::filesystem::path>, error::Error> list_dir(
        const std::filesystem::path & path
    ) override;

    std::expected<bool, error::Error> exists(const std::filesystem::path & path) override;

    std::expected<void, error::Error> create_dir_all(const std::filesystem::path & path) override;

    std::expected<void, error::Error> rename(
        const std::filesystem::path & from,
        const std::filesystem::path & to
    ) override;

    std::expected<void, error::Error> replace_file_atomic(
        const std::filesystem::path & from,
        const std::filesystem::path & to
    ) override;

    std::expected<void, error::Error> remove(const std::filesystem::path & path) override;

    std::expected<void, error::Error> sync_directory(const std::filesystem::path & path) override;
};

} // namespace litedb::core::filesystem::backend

#endif // _WIN32
