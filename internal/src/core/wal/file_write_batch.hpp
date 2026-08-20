#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <span>
#include <vector>

#include "core/filesystem/filesystem.hpp"
#include "core/wal/wal_error.hpp"
#include "core/wal/wal_types.hpp"

namespace litedb::core::wal
{

using FileWriteAppliedHook = std::function<bool(std::size_t, const FileWrite &)>;

// 文件写入批处理
class FileWriteBatch final
{
public:
    // 添加文件写入
    void add(FileWrite write);

    // 获取写入列表
    [[nodiscard]]
    const std::vector<FileWrite> & writes() const noexcept;

    // 原地规范化并校验写入批次。
    // 该操作在 WAL 追加前执行，确保冲突、重叠、范围和生命周期约束先于持久化检查。
    [[nodiscard]]
    std::expected<void, WalError> normalize();

    // 将批处理应用到数据目录
    [[nodiscard]]
    std::expected<void, WalError> apply(
        const std::filesystem::path & data_directory,
        filesystem::FileSystem & filesystem,
        bool sync,
        const FileWriteAppliedHook & applied_hook = {}
    ) const;

    // 解析文件目标路径
    [[nodiscard]]
    static std::expected<std::filesystem::path, WalError>
    resolve_target(const std::filesystem::path & data_directory, const FileTarget & target);

private:
    std::vector<FileWrite> writes_; // 写入列表
};

} // namespace litedb::core::wal
