#pragma once

#include <string>

namespace litedb::core::meta
{

/**
 * @brief 元数据错误码
 */
enum class MetaErrorCode
{
    InvalidArgument,        ///< 无效参数
    DatabaseNotFound,       ///< 数据库不存在
    CollectionNotFound,     ///< 集合不存在
    ColumnNotFound,         ///< 列不存在
    IndexNotFound,          ///< 索引不存在
    VectorIndexNotFound,    ///< 向量索引不存在
    DuplicateDatabase,      ///< 重复数据库
    DuplicateCollection,    ///< 重复集合
    DuplicateColumn,        ///< 重复列
    DuplicateIndex,         ///< 重复索引
    DuplicateVectorIndex,   ///< 重复向量索引
    MultiplePrimaryKeys,    ///< 多个主键
};

/**
 * @brief 元数据错误
 */
struct MetaError
{
    MetaErrorCode code;     ///< 错误码
    std::string message;    ///< 错误消息
};

} // namespace litedb::core::meta
