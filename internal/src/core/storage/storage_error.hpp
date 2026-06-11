#pragma once

#include <string>

namespace litedb::core::storage
{

/**
 * @brief 存储错误码
 */
enum class StorageErrorCode
{
    CollectionAlreadyExists,    ///< 集合已存在
    CollectionNotFound,         ///< 集合不存在
    RecordNotFound,             ///< 记录不存在
    ValueCountMismatch,         ///< 值数量不匹配
    TypeMismatch,               ///< 类型不匹配
    NullConstraintViolation,    ///< 空约束违反
    IoError,                    ///< IO 错误
    InvalidStorageFormat,       ///< 无效的存储格式
    InvalidStorageState,        ///< 无效的存储状态
};

/**
 * @brief 存储错误
 */
struct StorageError
{
    StorageErrorCode code;      ///< 错误码
    std::string message;        ///< 错误消息
};

} // namespace litedb::core::storage
