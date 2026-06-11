#pragma once

#include <expected>
#include <filesystem>

#include "core/storage/storage_error.hpp"

namespace litedb::core::persistence
{

class ManifestStore
{
public:
    explicit ManifestStore(std::filesystem::path data_dir);

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
};

} // namespace litedb::core::persistence
