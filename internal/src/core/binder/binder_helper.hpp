#pragma once

#include <memory>
#include <optional>
#include <string>

#include "core/binder/binder_error.hpp"
#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/binder/bound/statement/bound_statement.hpp"
#include "core/catalog/catalog_entry.hpp"
#include "core/common/logical_type.hpp"
#include "core/parser/ast/ast_node.hpp"
#include "core/parser/ast/statement/create_index_statement.hpp"
#include "core/parser/token.hpp"

namespace litedb::core::binder
{

/**
 * @brief 创建绑定错误
 * @param code 错误码
 * @param location 错误位置
 * @param message 错误消息
 * @return 绑定错误
 */
[[nodiscard]]
BinderError make_binder_error(BinderErrorCode code, parser::ast::AstNodeLocation location, std::string message);

/**
 * @brief 创建逻辑类型
 * @param id 逻辑类型 ID
 * @param parameter 逻辑类型参数
 * @return 逻辑类型
 */
[[nodiscard]]
common::LogicalType type(common::LogicalTypeId id, std::optional<std::size_t> parameter = std::nullopt);

/**
 * @brief 判断两个逻辑类型是否相同
 * @param left 左逻辑类型
 * @param right 右逻辑类型
 * @return 是否相同
 */
[[nodiscard]]
bool same_type(const common::LogicalType & left, const common::LogicalType & right);

/**
 * @brief 判断逻辑类型是否为数值类型
 * @param value 逻辑类型
 * @return 是否为数值类型
 */
[[nodiscard]]
bool is_numeric(const common::LogicalType & value);

/**
 * @brief 判断逻辑类型是否为布尔类型
 * @param value 逻辑类型
 * @return 是否为布尔类型
 */
[[nodiscard]]
bool is_boolean(const common::LogicalType & value);

/**
 * @brief 判断逻辑类型是否为字符串类型
 * @param value 逻辑类型
 * @return 是否为字符串类型
 */
[[nodiscard]]
bool is_varchar(const common::LogicalType & value);

/**
 * @brief 获取逻辑类型名称
 * @param value 逻辑类型
 * @return 逻辑类型名称
 */
[[nodiscard]]
std::string type_name(const common::LogicalType & value);

/**
 * @brief 获取逻辑类型的数字排名
 * @param value 逻辑类型
 * @return 数字排名
 */
[[nodiscard]]
int numeric_rank(const common::LogicalType & value);

/**
 * @brief 判断是否可以转换逻辑类型
 * @param source 源逻辑类型
 * @param target 目标逻辑类型
 * @return 是否可以转换
 */
[[nodiscard]]
bool can_cast(const common::LogicalType & source, const common::LogicalType & target);

/**
 * @brief 获取公共数值类型
 * @param left 左逻辑类型
 * @param right 右逻辑类型
 * @return 公共数值类型
 */
[[nodiscard]]
common::LogicalType common_numeric_type(const common::LogicalType & left, const common::LogicalType & right);

/**
 * @brief 判断是否可以比较逻辑类型
 * @param left 左逻辑类型
 * @param right 右逻辑类型
 * @param op 操作符
 * @return 是否可以比较
 */
[[nodiscard]]
bool can_compare(const common::LogicalType & left, const common::LogicalType & right, parser::TokenType op);

/**
 * @brief 判断是否需要转换逻辑类型
 * @param expression 表达式
 * @param target_type 目标逻辑类型
 * @return 是否需要转换
 */
[[nodiscard]]
std::unique_ptr<bound::BoundExpression> cast_if_needed(
    std::unique_ptr<bound::BoundExpression> expression,
    common::LogicalType target_type
);

/**
 * @brief 从列定义创建绑定列
 * @param column 列定义
 * @return 绑定列
 */
[[nodiscard]]
bound::BoundColumn bound_column_from_entry(const catalog::ColumnEntry & column);

/**
 * @brief 获取索引类型
 * @param method 索引方法
 * @return 索引类型
 */
[[nodiscard]]
catalog::CatalogIndexKind catalog_index_kind(parser::ast::CreateIndexMethod method);

} // namespace litedb::core::binder
