#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

namespace litedb::core::common
{

// 逻辑类型 ID
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

// 逻辑类型
// id 表示逻辑类型
// parameter 表示逻辑类型的额外参数，如：VARCHAR(n) 中的 n
struct LogicalType
{
    LogicalTypeId id;
    std::optional<std::size_t> parameter;
};

} // namespace litedb::core::common
