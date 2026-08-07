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

/**
 * @brief 文件写入批处理
 */
class FileWriteBatch final
{
public:
    /**
     * @brief 添加文件写入
     * @param write 文件写入记录
     */
    void add(FileWrite write);

    /**
     * @brief 获取写入列表
     * @return 写入列表
     */
    [[nodiscard]]
    const std::vector<FileWrite> & writes() const noexcept;

    /**
     * @brief 判断批处理是否为空
     * @return 是否为空
     */
    [[nodiscard]]
    bool empty() const noexcept;

    /**
     * @brief 按批处理覆盖结果读取目标范围
     * @param target 文件目标
     * @param offset 起始偏移
     * @param base 基础数据
     * @return 覆盖后的数据
     */
    [[nodiscard]]
    std::expected<std::vector<std::byte>, WalError> read(
        const FileTarget & target,
        std::uint64_t offset,
        std::span<const std::byte> base
    ) const;

    /**
     * @brief 将批处理应用到数据目录
     * @param data_directory 数据目录
     * @param filesystem 文件系统
     * @param sync 是否同步刷盘
     * @return 结果
     */
    [[nodiscard]]
    std::expected<void, WalError> apply(
        const std::filesystem::path & data_directory,
        filesystem::FileSystem & filesystem,
        bool sync,
        const FileWriteAppliedHook & applied_hook = {}
    ) const;

    /**
     * @brief 解析文件目标路径
     * @param data_directory 数据目录
     * @param target 文件目标
     * @return 目标路径
     */
    [[nodiscard]]
    static std::filesystem::path resolve_target(
        const std::filesystem::path & data_directory,
        const FileTarget & target
    );

private:
    std::vector<FileWrite> writes_;    // 写入列表
};

} // namespace litedb::core::wal
