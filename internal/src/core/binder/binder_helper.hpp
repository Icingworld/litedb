#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "core/binder/binder_error.hpp"
#include "core/binder/bound/bound_column.hpp"
#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/common/logical_type.hpp"
#include "core/common/types.hpp"
#include "core/catalog/entry/column_entry.hpp"

namespace litedb::core::binder
{

// 创建绑定错误
[[nodiscard]]
BinderError make_binder_error(BinderErrorCode code, std::string_view message);

// 创建绑定错误
[[nodiscard]]
BinderError make_binder_error(
    BinderErrorCode code,
    parser::ast::AstNodeLocation location,
    std::string_view message
);

// 创建逻辑类型
[[nodiscard]]
common::LogicalType
type(common::LogicalTypeId id, std::optional<std::size_t> parameter = std::nullopt) noexcept;

// 判断两个逻辑类型是否相同
[[nodiscard]]
bool same_type(const common::LogicalType & left, const common::LogicalType & right) noexcept;

// 判断逻辑类型是否为数值类型
[[nodiscard]]
bool is_numeric(const common::LogicalType & value) noexcept;

// 判断逻辑类型是否为布尔类型
[[nodiscard]]
bool is_boolean(const common::LogicalType & value) noexcept;

// 判断逻辑类型是否为字符串类型
[[nodiscard]]
bool is_varchar(const common::LogicalType & value) noexcept;

// 将逻辑类型转换为字符串
[[nodiscard]]
std::string type_name(const common::LogicalType & value);

// 获取逻辑类型的数字排名
[[nodiscard]]
int numeric_rank(const common::LogicalType & value) noexcept;

// 判断是否可以转换逻辑类型
[[nodiscard]]
bool can_cast(const common::LogicalType & source, const common::LogicalType & target) noexcept;

// 获取公共数值类型
[[nodiscard]]
common::LogicalType
common_numeric_type(const common::LogicalType & left, const common::LogicalType & right) noexcept;

// 判断是否可以比较逻辑类型
[[nodiscard]]
bool can_compare(
    const common::LogicalType & left,
    const common::LogicalType & right,
    common::BinaryOperator op
) noexcept;

// 判断是否需要转换逻辑类型
[[nodiscard]]
std::unique_ptr<bound::BoundExpression>
cast_if_needed(std::unique_ptr<bound::BoundExpression> expression, common::LogicalType target_type);

// 从列定义创建绑定列
[[nodiscard]]
bound::BoundColumn bound_column_from_entry(const catalog::entry::ColumnEntry & column);

} // namespace litedb::core::binder
