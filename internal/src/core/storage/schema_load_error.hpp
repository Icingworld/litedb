#pragma once

#include <string>

namespace litedb::core::storage
{

/**
 * @brief Schema 加载错误码
 */
enum class SchemaLoadErrorCode
{
    DatabaseNotFound,
    CollectionNotFound,
};

/**
 * @brief Schema 加载错误
 */
struct SchemaLoadError
{
    SchemaLoadErrorCode code;
    std::string message;
};

} // namespace litedb::core::storage
