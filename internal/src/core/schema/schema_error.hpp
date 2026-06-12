#pragma once

#include <string>

namespace litedb::core::schema
{

/**
 * @brief Schema 错误码
 */
enum class SchemaErrorCode
{
    DatabaseNotFound,        ///< 数据库不存在
    CollectionNotFound,      ///< 集合不存在
};

/**
 * @brief Schema 错误
 */
struct SchemaError
{
    SchemaErrorCode code;   ///< 错误码
    std::string message;    ///< 错误消息
};

} // namespace litedb::core::schema
