#pragma once

namespace litedb::core::filesystem
{

// 文件访问方式
enum class FileAccess
{
    ReadOnly, // 只读
    WriteOnly, // 只写
    ReadWrite, // 读写
};

// 文件创建方式
enum class FileCreateMode
{
    OpenExisting, // 仅打开已有文件
    OpenOrCreate, // 打开已有文件，文件不存在时创建
    CreateNew, // 创建新文件，文件已存在时失败
    TruncateExisting, // 打开并清空已有文件，文件不存在时失败
    CreateOrTruncate, // 创建文件，文件已存在时清空
};

// 文件系统打开文件选项
// ReadOnly 不能与 TruncateExisting 或 CreateOrTruncate 组合
struct FileOpenOptions
{
    FileAccess access {FileAccess::ReadOnly}; // 访问方式
    FileCreateMode create_mode {FileCreateMode::OpenExisting}; // 创建方式
};

} // namespace litedb::core::filesystem
