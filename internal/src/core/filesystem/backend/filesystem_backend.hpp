#pragma once

#include <expected>
#include <filesystem>
#include <memory>
#include <vector>

#include "core/filesystem/backend/file_handle_backend.hpp"
#include "core/filesystem/file_open_options.hpp"
#include "core/filesystem/filesystem_error.hpp"

namespace litedb::core::filesystem::backend
{

// 文件系统后端
class FileSystemBackend
{
public:
    virtual ~FileSystemBackend() = default;

public:
    // 打开文件
    virtual std::expected<std::unique_ptr<FileHandleBackend>, FileSystemError>
    open(const std::filesystem::path & path, const FileOpenOptions & options) = 0;

    // 列出目录，返回直接目录项的名称，不返回带输入目录前缀的完整路径
    virtual std::expected<std::vector<std::filesystem::path>, FileSystemError> list_dir(
        const std::filesystem::path & path
    ) = 0;

    // 检查文件是否存在
    virtual std::expected<bool, FileSystemError> exists(const std::filesystem::path & path) = 0;

    // 创建目录及其所有父目录
    virtual std::expected<void, FileSystemError> create_dir_all(
        const std::filesystem::path & path
    ) = 0;

    // 原子地重命名文件或目录且不覆盖已有目标
    // 目标已存在时返回 AlreadyExists；无法提供该语义时返回 Unsupported
    virtual std::expected<void, FileSystemError>
    rename(const std::filesystem::path & from, const std::filesystem::path & to) = 0;

    // 原子地发布文件，目标文件存在时替换它
    // 源文件和目标文件必须位于同一文件系统。成功后调用方仍需同步父目录，
    // 才能保证目录项变更在掉电后持久化。
    virtual std::expected<void, FileSystemError>
    replace_file_atomic(const std::filesystem::path & from, const std::filesystem::path & to) = 0;

    // 删除文件或空目录；路径不存在时也成功
    virtual std::expected<void, FileSystemError> remove(const std::filesystem::path & path) = 0;

    // 将目录项变更同步到持久化存储；平台不支持时返回 Unsupported
    virtual std::expected<void, FileSystemError> sync_directory(
        const std::filesystem::path & path
    ) = 0;
};

} // namespace litedb::core::filesystem::backend
