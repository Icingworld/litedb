#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include "core/common/logical_type.hpp"
#include "core/common/types.hpp"

namespace litedb::core::common
{

// 判断两个逻辑类型是否相同
[[nodiscard]]
bool same_type(const LogicalType & left, const LogicalType & right) noexcept;

// 判断逻辑类型是否为数值类型
[[nodiscard]]
bool is_numeric(const LogicalType & type) noexcept;

// 判断逻辑类型是否为布尔类型
[[nodiscard]]
bool is_boolean(const LogicalType & type) noexcept;

// 判断逻辑类型是否为字符串类型
[[nodiscard]]
bool is_varchar(const LogicalType & type) noexcept;

// 获取逻辑类型名称
[[nodiscard]]
std::string type_name(const LogicalType & type);

// 获取数值类型的提升排名
[[nodiscard]]
int numeric_rank(const LogicalType & type) noexcept;

// 获取两个数值类型的公共类型
[[nodiscard]]
LogicalType common_numeric_type(const LogicalType & left, const LogicalType & right) noexcept;

// 判断是否可以隐式转换逻辑类型
[[nodiscard]]
bool can_implicitly_cast(const LogicalType & source, const LogicalType & target) noexcept;

// 获取隐式转换代价
[[nodiscard]]
std::optional<std::size_t>
implicit_cast_cost(const LogicalType & source, const LogicalType & target) noexcept;

// 判断两个逻辑类型是否可以按给定运算符比较
[[nodiscard]]
bool can_compare(const LogicalType & left, const LogicalType & right, BinaryOperator op) noexcept;

} // namespace litedb::core::common
