#pragma once

#include <optional>
#include <string>

#include "core/meta/meta_store_error.hpp"

namespace litedb::core::meta
{

/**
 * @brief 元数据引擎错误码
 */
enum class MetaEngineErrorCode
{
    InvalidArgument,
    InvalidSnapshot,
    DatabaseNotFound,
    CollectionNotFound,
    ColumnNotFound,
    IndexNotFound,
    VectorIndexNotFound,
    DuplicateDatabase,
    DuplicateCollection,
    DuplicateColumn,
    DuplicateIndex,
    DuplicateVectorIndex,
    StoreError,
};

/**
 * @brief 元数据引擎错误
 */
struct MetaEngineError
{
    MetaEngineErrorCode code;                        ///< 错误码
    std::string message;                             ///< 错误消息
    std::optional<MetaStoreErrorCode> store_code;    ///< 存储错误码
};

} // namespace litedb::core::meta
