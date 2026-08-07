#pragma once

#include <expected>
#include <filesystem>
#include <optional>

#include "core/filesystem/filesystem.hpp"
#include "core/meta/meta_error.hpp"
#include "core/meta/meta_snapshot.hpp"

namespace litedb::core::meta
{

/**
 * @brief 元数据存储
 */
class MetaStore
{
public:
    MetaStore(std::filesystem::path path, filesystem::FileSystem & filesystem);

public:
    /**
     * @brief 加载元数据快照
     * @return 元数据快照；文件不存在时返回 std::nullopt
     */
    [[nodiscard]]
    std::expected<std::optional<MetaSnapshot>, MetaError> load() const;

    /**
     * @brief 原子保存元数据快照
     * @param snapshot 元数据快照
     * @return 是否成功
     */
    [[nodiscard]]
    std::expected<void, MetaError> save(const MetaSnapshot & snapshot) const;

private:
    std::filesystem::path path_;            // 元数据文件路径
    filesystem::FileSystem * filesystem_;   // 文件系统
};

} // namespace litedb::core::meta
