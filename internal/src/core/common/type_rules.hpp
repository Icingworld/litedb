#pragma once

#include <string>
#include <cstddef>
#include <optional>

#include "core/common/logical_type.hpp"
#include "core/common/types.hpp"

namespace litedb::core::common
{

/**
 * @brief 判断两个逻辑类型是否相同
 * @param left 左逻辑类型
 * @param right 右逻辑类型
 * @return 是否相同
 */
[[nodiscard]]
bool same_type(
    const LogicalType & left,
    const LogicalType & right
) noexcept;

/**
 * @brief 判断逻辑类型是否为数值类型
 * @param type 逻辑类型
 * @return 是否为数值类型
 */
[[nodiscard]]
bool is_numeric(const LogicalType & type) noexcept;

/**
 * @brief 判断逻辑类型是否为布尔类型
 * @param type 逻辑类型
 * @return 是否为布尔类型
 */
[[nodiscard]]
bool is_boolean(const LogicalType & type) noexcept;

/**
 * @brief 判断逻辑类型是否为字符串类型
 * @param type 逻辑类型
 * @return 是否为字符串类型
 */
[[nodiscard]]
bool is_varchar(const LogicalType & type) noexcept;

/**
 * @brief 获取逻辑类型名称
 * @param type 逻辑类型
 * @return 逻辑类型名称
 */
[[nodiscard]]
std::string type_name(const LogicalType & type);

/**
 * @brief 获取数值类型的提升排名
 * @param type 逻辑类型
 * @return 数值排名，非数值类型返回 0
 */
[[nodiscard]]
int numeric_rank(const LogicalType & type) noexcept;

/**
 * @brief 获取两个数值类型的公共类型
 * @param left 左逻辑类型
 * @param right 右逻辑类型
 * @return 公共数值类型
 */
[[nodiscard]]
LogicalType common_numeric_type(
    const LogicalType & left,
    const LogicalType & right
) noexcept;

/**
 * @brief 判断是否可以隐式转换逻辑类型
 * @param source 源逻辑类型
 * @param target 目标逻辑类型
 * @return 是否可以隐式转换
 */
[[nodiscard]]
bool can_implicitly_cast(
    const LogicalType & source,
    const LogicalType & target
) noexcept;

/**
 * @brief 获取隐式转换代价
 *
 * 返回空值表示不能进行隐式转换。代价越低，表示匹配越精确。
 */
[[nodiscard]]
std::optional<std::size_t> implicit_cast_cost(
    const LogicalType & source,
    const LogicalType & target
) noexcept;

/**
 * @brief 判断两个逻辑类型是否可以按给定运算符比较
 * @param left 左逻辑类型
 * @param right 右逻辑类型
 * @param op 二元比较运算符
 * @return 是否可以比较
 */
[[nodiscard]]
bool can_compare(
    const LogicalType & left,
    const LogicalType & right,
    BinaryOperator op
) noexcept;

} // namespace litedb::core::common
