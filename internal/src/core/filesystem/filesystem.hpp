#pragma once

#include <expected>
#include <filesystem>
#include <memory>
#include <vector>

#include "core/filesystem/backend/filesystem_backend.hpp"
#include "core/filesystem/filesystem_error.hpp"
#include "core/filesystem/file_handle.hpp"

namespace litedb::core::filesystem
{

/**
 * @brief 文件系统
 */
class FileSystem
{
public:
    explicit FileSystem(std::unique_ptr<backend::FileSystemBackend> backend);

    FileSystem(const FileSystem &) = delete;

    FileSystem & operator=(const FileSystem &) = delete;

    FileSystem(FileSystem &&) noexcept;

    FileSystem & operator=(FileSystem &&) noexcept;

    ~FileSystem();

public:
    /**
     * @brief 打开文件
     * @param path 文件路径
     * @param options 打开选项
     * @return 结果
     */
    [[nodiscard]]
    std::expected<FileHandle, FileSystemError> open(
        const std::filesystem::path & path,
        const backend::FileOpenOptions & options = {}
    );

    /**
     * @brief 列出目录
     * @param path 目录路径
     * @return 结果
     */
    [[nodiscard]]
    std::expected<std::vector<std::filesystem::path>, FileSystemError> list_dir(
        const std::filesystem::path & path
    );

    /**
     * @brief 检查文件是否存在
     * @param path 文件路径
     * @return 结果
     */
    [[nodiscard]]
    std::expected<bool, FileSystemError> exists(const std::filesystem::path & path);

    /**
     * @brief 创建目录及其所有父目录
     * @param path 目录路径
     * @return 结果
     */
    [[nodiscard]]
    std::expected<void, FileSystemError> create_dir_all(const std::filesystem::path & path);

    /**
     * @brief 重命名文件或目录
     * @param from 原文件或目录路径
     * @param to 新文件或目录路径
     * @return 结果
     */
    [[nodiscard]]
    std::expected<void, FileSystemError> rename(
        const std::filesystem::path & from,
        const std::filesystem::path & to
    );

    /**
     * @brief 删除文件或目录
     * @param path 文件或目录路径
     * @return 结果
     */
    [[nodiscard]]
    std::expected<void, FileSystemError> remove(const std::filesystem::path & path);

    /**
     * @brief 将目录项变更同步到持久化存储
     * @param path 目录路径
     * @return 结果；平台不支持目录同步时返回 Unsupported
     */
    [[nodiscard]]
    std::expected<void, FileSystemError> sync_directory(const std::filesystem::path & path);

private:
    std::unique_ptr<backend::FileSystemBackend> backend_;   ///< 文件系统后端
};

} // namespace litedb::core::filesystem
