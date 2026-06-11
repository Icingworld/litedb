#pragma once

#include <expected>
#include <filesystem>

#include "core/catalog/catalog_snapshot.hpp"
#include "core/storage/storage_error.hpp"

namespace litedb::core::persistence
{

class CatalogStore
{
public:
    explicit CatalogStore(std::filesystem::path path);

public:
    [[nodiscard]]
    std::expected<catalog::CatalogSnapshot, storage::StorageError> load_or_empty() const;

    [[nodiscard]]
    std::expected<void, storage::StorageError> save(const catalog::CatalogSnapshot & snapshot) const;

private:
    std::filesystem::path path_;            ///< 路径
};

} // namespace litedb::core::persistence
