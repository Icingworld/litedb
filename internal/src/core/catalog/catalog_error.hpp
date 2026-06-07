#pragma once

#include <cstdint>
#include <string>

namespace litedb::core::catalog
{

/**
 * @brief 目录错误码
 */
enum class CatalogErrorCode : std::uint8_t
{
    InvalidArgument,                  ///< 无效参数
    DatabaseNotFound,                 ///< 数据库不存在
    CollectionNotFound,               ///< 集合不存在
    DuplicateDatabase,                ///< 数据库已存在
    DuplicateCollection,              ///< 集合已存在
    DuplicateColumn,                  ///< 列已存在
    MultiplePrimaryKeys,              ///< 多个主键
};

/**
 * @brief 目录错误
 */
struct CatalogError
{
    CatalogErrorCode code;             ///< 错误码
    std::string message;               ///< 错误消息
};

} // namespace litedb::core::catalog
