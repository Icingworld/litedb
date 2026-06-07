#pragma once

#include <cstdint>
#include <optional>
#include <cstddef>

namespace litedb::core::common
{

/**
 * @brief 逻辑类型 ID
 */
enum class LogicalTypeId : std::uint8_t
{
    Null,
    Boolean,
    Integer,
    BigInt,
    Float,
    Double,
    Varchar,
    Vector,
};

/**
 * @brief 逻辑类型
 */
struct LogicalType
{
    LogicalTypeId id;                           ///< 逻辑类型 ID
    std::optional<std::size_t> parameter;       ///< 逻辑类型参数
};

} // namespace litedb::core::common
