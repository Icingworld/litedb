#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

namespace litedb::core::common
{

// 逻辑类型 ID
enum class LogicalTypeId : std::uint8_t
{
    Null = 0,
    Boolean = 1,
    Integer = 2,
    BigInt = 3,
    Float = 4,
    Double = 5,
    Varchar = 6,
    Vector = 7,
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
