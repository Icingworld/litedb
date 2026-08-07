#pragma once

#include <expected>
#include <filesystem>
#include <memory>
#include <vector>

#include "core/filesystem/filesystem_error.hpp"
#include "core/filesystem/file_handle.hpp"
#include "core/filesystem/file_open_options.hpp"

namespace litedb::core::filesystem
{

namespace backend
{

class FileSystemBackend;

} // namespace backend

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
    std::expected<FileHandle, error::Error> open(
        const std::filesystem::path & path,
        const FileOpenOptions & options = {}
    );

    /**
     * @brief 列出目录
     * @param path 目录路径
     * @return 结果
     */
    [[nodiscard]]
    std::expected<std::vector<std::filesystem::path>, error::Error> list_dir(
        const std::filesystem::path & path
    );

    /**
     * @brief 检查文件是否存在
     * @param path 文件路径
     * @return 结果
     */
    [[nodiscard]]
    std::expected<bool, error::Error> exists(const std::filesystem::path & path);

    /**
     * @brief 创建目录及其所有父目录
     * @param path 目录路径
     * @return 结果
     */
    [[nodiscard]]
    std::expected<void, error::Error> create_dir_all(const std::filesystem::path & path);

    /**
     * @brief 在目标不存在时重命名文件或目录
     * @param from 原文件或目录路径
     * @param to 新文件或目录路径
     * @return 结果；目标已存在时返回 AlreadyExists
     */
    [[nodiscard]]
    std::expected<void, error::Error> rename(
        const std::filesystem::path & from,
        const std::filesystem::path & to
    );

    /**
     * @brief 原子地发布文件，目标文件存在时替换它
     *
     * 源文件必须已完成写入和同步，且必须与目标文件位于同一文件系统。
     * 成功后调用方应同步目标文件的父目录。
     */
    [[nodiscard]]
    std::expected<void, error::Error> replace_file_atomic(
        const std::filesystem::path & from,
        const std::filesystem::path & to
    );

    /**
     * @brief 删除文件或目录
     * @param path 文件或目录路径
     * @return 结果
     */
    [[nodiscard]]
    std::expected<void, error::Error> remove(const std::filesystem::path & path);

    /**
     * @brief 将目录项变更同步到持久化存储
     * @param path 目录路径
     * @return 结果；平台不支持目录同步时返回 Unsupported
     */
    [[nodiscard]]
    std::expected<void, error::Error> sync_directory(const std::filesystem::path & path);

private:
    std::unique_ptr<backend::FileSystemBackend> backend_;   // 文件系统后端
};

} // namespace litedb::core::filesystem
