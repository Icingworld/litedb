#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>

#include "core/error/error.hpp"

namespace litedb::core::filesystem
{

// 文件系统错误
enum class FileSystemErrorCode : std::uint8_t
{
    NotFound = 0, // 文件或目录不存在
    AlreadyExists = 1, // 文件或目录已存在
    PermissionDenied = 2, // 权限不足
    InvalidArgument = 3, // 参数无效
    InvalidPath = 4, // 路径无效
    NotAFile = 5, // 路径不是文件
    NotADirectory = 6, // 路径不是目录
    DirectoryNotEmpty = 7, // 目录非空
    ReadOnly = 8, // 文件或文件系统只读
    NoSpace = 9, // 存储空间不足
    ResourceBusy = 10, // 资源正被占用
    Unsupported = 11, // 当前文件系统不支持该操作
    IoError = 12, // 其他输入输出错误
    ClosedHandle = 13, // 文件句柄已关闭
    InvalidState = 14, // 对象当前没有可用的后端
};

/**
 * @brief 文件系统错误上下文
 */
struct FileSystemErrorContext
{
    std::string operation; // 失败的文件系统操作
    std::filesystem::path path; // 主要操作路径
    std::filesystem::path related_path; // rename/replace 等操作的第二路径
    std::error_code native_code; // 原始平台错误码
};

using FileSystemError = error::Error;

} // namespace litedb::core::filesystem

namespace litedb::core::error
{

/**
 * @brief 文件系统错误类型特化
 */
template <>
struct ErrorTraits<filesystem::FileSystemErrorCode>
{
    static constexpr ErrorCategory category = ErrorCategory::FileSystem;
};

} // namespace litedb::core::error
