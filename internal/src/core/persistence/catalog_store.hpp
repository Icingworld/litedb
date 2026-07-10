#pragma once

#include <expected>
#include <filesystem>

#include "core/catalog/catalog_snapshot.hpp"
#include "core/filesystem/filesystem.hpp"
#include "core/storage/storage_error.hpp"

namespace litedb::core::persistence
{

class CatalogStore
{
public:
    CatalogStore(std::filesystem::path path, filesystem::FileSystem & filesystem);

public:
    [[nodiscard]]
    std::expected<catalog::CatalogSnapshot, storage::StorageError> load_or_empty() const;

    [[nodiscard]]
    std::expected<void, storage::StorageError> save(const catalog::CatalogSnapshot & snapshot) const;

private:
    std::filesystem::path path_;            ///< 路径
    filesystem::FileSystem * filesystem_;   ///< 文件系统
};

} // namespace litedb::core::persistence
