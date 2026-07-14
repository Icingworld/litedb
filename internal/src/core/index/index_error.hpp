#pragma once

#include <string>

namespace litedb::core::index
{

/**
 * @brief 索引错误码
 */
enum class IndexErrorCode
{
    UnsupportedKeyType,     ///< 不支持的键类型
    InvalidKeyValue,        ///< 无效的键值
    KeyTypeMismatch,        ///< 键类型与索引列类型不匹配
    UnsupportedRangeScan,   ///< 不支持的范围扫描
    KeyNotFound,            ///< 键不存在
    RecordNotFound,         ///< 记录不存在
    DuplicateEntry,         ///< 键值与记录 ID 对已存在
    DuplicateKey,           ///< 唯一索引键重复
    IndexAlreadyExists,     ///< 索引已存在
    IndexNotFound,          ///< 索引不存在
    InvalidIndexColumn,     ///< 索引列无效或不可索引
    StorageError,           ///< 存储扫描失败
};

/**
 * @brief 索引错误
 */
struct IndexError
{
    IndexErrorCode code;    ///< 错误码
    std::string message;    ///< 错误信息
};

} // namespace litedb::core::index
