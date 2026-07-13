#pragma once

#include <expected>
#include <filesystem>
#include <memory>
#include <vector>

#include "core/filesystem/backend/file_handle_backend.hpp"
#include "core/filesystem/filesystem_error.hpp"

namespace litedb::core::filesystem::backend
{

/**
 * @brief 文件访问方式
 */
enum class FileAccess
{
    ReadOnly,                  ///< 只读
    WriteOnly,                 ///< 只写
    ReadWrite,                 ///< 读写
};

/**
 * @brief 文件创建方式
 */
enum class FileCreateMode
{
    OpenExisting,              ///< 仅打开已有文件
    OpenOrCreate,              ///< 打开已有文件，文件不存在时创建
    CreateNew,                 ///< 创建新文件，文件已存在时失败
    TruncateExisting,          ///< 打开并清空已有文件，文件不存在时失败
    CreateOrTruncate,          ///< 创建文件，文件已存在时清空
};

/**
 * @brief 文件系统打开文件选项
 */
struct FileOpenOptions
{
    FileAccess access {FileAccess::ReadOnly};                  ///< 访问方式
    FileCreateMode create_mode {FileCreateMode::OpenExisting}; ///< 创建方式
};

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
     * @brief 重命名文件或目录
     * @param from 原路径
     * @param to 新路径
     * @return 结果
     */
    virtual std::expected<void, FileSystemError> rename(
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
