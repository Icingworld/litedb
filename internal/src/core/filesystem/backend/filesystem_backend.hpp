#pragma once

#include <expected>
#include <filesystem>
#include <memory>
#include <vector>

#include "core/filesystem/backend/file_handle_backend.hpp"
#include "core/filesystem/filesystem_error.hpp"
#include "core/filesystem/file_open_options.hpp"

namespace litedb::core::filesystem::backend
{

/**
 * @brief 文件系统后端
 */
class FileSystemBackend
{
public:
    virtual ~FileSystemBackend() = default;

public:
    /**
     * @brief 打开文件
     * @param path 文件路径
     * @param options 打开选项
     * @return 结果
     */
    virtual std::expected<std::unique_ptr<FileHandleBackend>, FileSystemError> open(
        const std::filesystem::path & path,
        const FileOpenOptions & options
    ) = 0;

    /**
     * @brief 列出目录
     * @param path 目录路径
     * @return 结果
     */
    virtual std::expected<std::vector<std::filesystem::path>, FileSystemError> list_dir(
        const std::filesystem::path & path
    ) = 0;

    /**
     * @brief 检查文件是否存在
     * @param path 文件路径
     * @return 结果
     */
    virtual std::expected<bool, FileSystemError> exists(const std::filesystem::path & path) = 0;

    /**
     * @brief 创建目录及其所有父目录
     * @param path 目录路径
     * @return 结果
     */
    virtual std::expected<void, FileSystemError> create_dir_all(const std::filesystem::path & path) = 0;

    /**
     * @brief 在目标不存在时重命名文件或目录
     * @param from 原路径
     * @param to 新路径
     * @return 结果；目标已存在时返回 AlreadyExists
     */
    virtual std::expected<void, FileSystemError> rename(
        const std::filesystem::path & from,
        const std::filesystem::path & to
    ) = 0;

    /**
     * @brief 原子地发布文件，目标文件存在时替换它
     * @param from 已完整写入并同步的源文件
     * @param to 目标文件
     * @return 结果
     *
     * 源文件和目标文件必须位于同一文件系统。成功后调用方仍需同步父目录，
     * 才能保证目录项变更在掉电后持久化。
     */
    virtual std::expected<void, FileSystemError> replace_file_atomic(
        const std::filesystem::path & from,
        const std::filesystem::path & to
    ) = 0;

    /**
     * @brief 删除文件或目录
     * @param path 路径
     * @return 结果
     */
    virtual std::expected<void, FileSystemError> remove(const std::filesystem::path & path) = 0;

    /**
     * @brief 同步目录
     * @param path 目录路径
     * @return 结果
     */
    virtual std::expected<void, FileSystemError> sync_directory(
        const std::filesystem::path & path
    ) = 0;
};

} // namespace litedb::core::filesystem::backend
