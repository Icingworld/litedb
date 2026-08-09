#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "core/common/logical_type.hpp"

namespace litedb::core::common
{

// 空值类型
using NullValue = std::monostate;

// 向量值类型
using VectorValue = std::vector<double>;

// 值数据类型
using ValueData = std::
    variant<NullValue, bool, std::int32_t, std::int64_t, float, double, std::string, VectorValue>;

// 逻辑值
class Value
{
public:
    Value();

    explicit Value(ValueData data);

public:
    // 获取空值
    [[nodiscard]]
    static Value null();

    // 是否为空
    [[nodiscard]]
    bool is_null() const noexcept;

    // 获取值数据
    [[nodiscard]]
    const ValueData & data() const noexcept;

    // 是否匹配类型
    [[nodiscard]]
    bool matches_type(const LogicalType & type) const noexcept;

private:
    ValueData data_;
};

// 将值转换为字符串
[[nodiscard]]
std::string value_to_string(const Value & value);

} // namespace litedb::core::common
