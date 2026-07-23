#pragma once

#include <filesystem>
#include <string>
#include <system_error>

namespace litedb::core::filesystem
{

/**
 * @brief 文件系统错误
 */
enum class FileSystemErrorCode
{
    NotFound,              ///< 文件或目录不存在
    AlreadyExists,         ///< 文件或目录已存在
    PermissionDenied,      ///< 权限不足
    InvalidArgument,       ///< 参数无效
    InvalidPath,           ///< 路径无效
    NotAFile,              ///< 路径不是文件
    NotADirectory,         ///< 路径不是目录
    DirectoryNotEmpty,     ///< 目录非空
    ReadOnly,              ///< 文件或文件系统只读
    NoSpace,               ///< 存储空间不足
    ResourceBusy,          ///< 资源正被占用
    Unsupported,           ///< 当前文件系统不支持该操作
    IoError,               ///< 其他输入输出错误
};

/**
 * @brief 文件系统错误
 */
struct FileSystemError
{
    FileSystemErrorCode code;                 ///< 跨平台错误码
    std::string message;                      ///< 可直接展示的错误信息
    std::string operation;                    ///< 失败的文件系统操作
    std::filesystem::path path;               ///< 主要操作路径
    std::filesystem::path related_path;       ///< rename/replace 等操作的第二路径
    std::error_code native_code;              ///< 原始平台错误码
};

} // namespace litedb::core::filesystem
