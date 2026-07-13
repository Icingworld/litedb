#pragma once

#include <optional>
#include <string>
#include <utility>

namespace litedb::core::storage
{

/**
 * @brief 持久化存储器错误码
 */
enum class StorageStoreErrorCode
{
    FileSystemError,          ///< 文件系统错误
    IoError,                  ///< IO 错误
    InvalidFormat,            ///< 格式错误
    UnsupportedVersion,       ///< 不支持的版本
    CorruptedPage,            ///< 损坏的页
    RecordNotFound,           ///< 记录未找到
    RecordTooLarge,           ///< 记录太大
    InvalidStoreState,        ///< 存储器状态无效
};

/**
 * @brief 持久化存储器错误
 */
struct StorageStoreError
{
    StorageStoreErrorCode code;     ///< 错误码
    std::string message;            ///< 错误消息
};

/**
 * @brief 持久化存储错误码
 */
enum class StorageErrorCode
{
    CollectionAlreadyExists,        ///< 集合已存在
    CollectionNotFound,             ///< 集合未找到
    CollectionStoreAlreadyExists,   ///< 集合存储已存在
    CollectionStoreNotFound,        ///< 集合存储未找到
    RecordNotFound,                 ///< 记录未找到
    ValueCountMismatch,             ///< 值数量不匹配
    TypeMismatch,                   ///< 类型不匹配
    NullConstraintViolation,        ///< 空约束违反
    ValueTooLarge,                  ///< 值太大
    RecordTooLarge,                 ///< 记录太大
    StoreError,                     ///< 存储器错误
};

/**
 * @brief 持久化存储错误
 */
struct StorageError
{
    StorageErrorCode code;          ///< 错误码
    std::string message;            ///< 错误消息
    std::optional<StorageStoreErrorCode> storage_store_code;    ///< 持久化存储器错误码
};

/**
 * @brief 从持久化存储器错误转换为持久化存储错误
 * @param error 持久化存储器错误
 * @return 持久化存储错误
 */
[[nodiscard]]
inline StorageError from_storage_store_error(StorageStoreError error)
{
    auto code = StorageErrorCode::StoreError;
    if (error.code == StorageStoreErrorCode::RecordNotFound) {
        code = StorageErrorCode::RecordNotFound;
    } else if (error.code == StorageStoreErrorCode::RecordTooLarge) {
        code = StorageErrorCode::RecordTooLarge;
    }
    return {code, std::move(error.message), error.code};
}

} // namespace litedb::core::storage
