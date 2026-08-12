#pragma once

#include <expected>
#include <filesystem>
#include <optional>

#include "core/filesystem/filesystem.hpp"
#include "core/catalog/catalog_error.hpp"
#include "core/catalog/catalog_snapshot.hpp"

namespace litedb::core::catalog
{

/**
 * @brief 元数据存储
 */
class CatalogStore
{
public:
    CatalogStore(std::filesystem::path path, filesystem::FileSystem & filesystem);

public:
    /**
     * @brief 加载元数据快照
     * @return 元数据快照；文件不存在时返回 std::nullopt
     */
    [[nodiscard]]
    std::expected<std::optional<CatalogSnapshot>, CatalogError> load() const;

    /**
     * @brief 原子保存元数据快照
     * @param snapshot 元数据快照
     * @return 是否成功
     */
    [[nodiscard]]
    std::expected<void, CatalogError> save(const CatalogSnapshot & snapshot) const;

private:
    std::filesystem::path path_;            // 元数据文件路径
    filesystem::FileSystem * filesystem_;   // 文件系统
};

} // namespace litedb::core::catalog
