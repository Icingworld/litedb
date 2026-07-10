#pragma once

#include <expected>
#include <filesystem>

#include "core/filesystem/filesystem.hpp"
#include "core/storage/storage_error.hpp"

namespace litedb::core::persistence
{

class ManifestStore
{
public:
    ManifestStore(std::filesystem::path data_dir, filesystem::FileSystem & filesystem);

public:
    [[nodiscard]]
    std::expected<void, storage::StorageError> ensure_initialized() const;

    [[nodiscard]]
    const std::filesystem::path & data_dir() const noexcept;

    [[nodiscard]]
    std::filesystem::path catalog_path() const;

    [[nodiscard]]
    std::filesystem::path collections_dir() const;

private:
    std::filesystem::path data_dir_;            ///< 数据目录
    filesystem::FileSystem * filesystem_;       ///< 文件系统
};

} // namespace litedb::core::persistence
