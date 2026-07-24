#pragma once

#include <cstdint>

#include "core/error/error.hpp"

namespace litedb::core::storage
{

/**
 * @brief Schema 加载错误码
 */
enum class SchemaLoadErrorCode : std::uint8_t
{
    DatabaseNotFound,
    CollectionNotFound,
};

/**
 * @brief Schema 加载错误
 */
using SchemaLoadError = error::Error;

} // namespace litedb::core::storage

namespace litedb::core::error
{
template <>
struct ErrorTraits<storage::SchemaLoadErrorCode>
{
    static constexpr ErrorCategory category = ErrorCategory::Storage;
};
} // namespace litedb::core::error
