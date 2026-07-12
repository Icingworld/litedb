#pragma once

#include <string>

namespace litedb::core::meta
{

/**
 * @brief 元数据存储错误码
 */
enum class MetaStoreErrorCode
{
    FileSystemError,       ///< 文件系统操作失败
    UnexpectedEof,         ///< 元数据文件意外结束
    InvalidFormat,         ///< 元数据文件格式无效
    UnsupportedVersion,    ///< 不支持的元数据格式版本
    ValueTooLarge,         ///< 待编码或解码的值过大
};

/**
 * @brief 元数据存储错误
 */
struct MetaStoreError
{
    MetaStoreErrorCode code;
    std::string message;
};

} // namespace litedb::core::meta
