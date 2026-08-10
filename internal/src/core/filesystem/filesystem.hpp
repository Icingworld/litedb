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

// 文件系统
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
    // 打开文件
    [[nodiscard]]
    std::expected<FileHandle, FileSystemError> open(
        const std::filesystem::path & path,
        const FileOpenOptions & options = {}
    );

    // 列出目录
    [[nodiscard]]
    std::expected<std::vector<std::filesystem::path>, FileSystemError> list_dir(
        const std::filesystem::path & path
    );

    // 检查文件是否存在
    [[nodiscard]]
    std::expected<bool, FileSystemError> exists(const std::filesystem::path & path);

    // 创建目录及其所有父目录
    [[nodiscard]]
    std::expected<void, FileSystemError> create_dir_all(const std::filesystem::path & path);

    // 在目标不存在时重命名文件或目录
    [[nodiscard]]
    std::expected<void, FileSystemError> rename(
        const std::filesystem::path & from,
        const std::filesystem::path & to
    );

    // 原子地发布文件，目标文件存在时替换它
    // 源文件必须已完成写入和同步，且必须与目标文件位于同一文件系统
    // 成功后调用方应同步目标文件的父目录
    [[nodiscard]]
    std::expected<void, FileSystemError> replace_file_atomic(
        const std::filesystem::path & from,
        const std::filesystem::path & to
    );

    // 删除文件或目录
    [[nodiscard]]
    std::expected<void, FileSystemError> remove(const std::filesystem::path & path);

    // 将目录项变更同步到持久化存储
    [[nodiscard]]
    std::expected<void, FileSystemError> sync_directory(const std::filesystem::path & path);

private:
    std::unique_ptr<backend::FileSystemBackend> backend_;   // 文件系统后端
};

} // namespace litedb::core::filesystem
