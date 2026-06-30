#pragma once

#include <string>

#include "core/parser/ast/ast_node.hpp"

namespace litedb::core::binder
{

/**
 * @brief 绑定错误码
 */
enum class BinderErrorCode
{
    UnsupportedStatement,      ///< 不支持的语句
    UnsupportedExpression,     ///< 不支持的表达式
    DatabaseNotSelected,       ///< 数据库未选择
    DatabaseNotFound,          ///< 数据库不存在
    CollectionNotFound,        ///< 集合不存在
    ColumnNotFound,            ///< 列不存在
    IndexNotFound,             ///< 索引不存在
    DuplicateColumn,           ///< 列已存在
    AmbiguousAlias,            ///< 别名不明确
    InvalidQualifier,          ///< 无效的限定符
    InvalidType,               ///< 无效的类型
    InvalidValueCount,         ///< 无效的值数量
    NotNullable,               ///< 不能为 NULL
};

/**
 * @brief 绑定错误
 */
struct BinderError
{
    BinderErrorCode code;                   ///< 错误码
    parser::ast::AstNodeLocation location;  ///< 错误位置
    std::string message;                    ///< 错误消息
};

} // namespace litedb::core::binder
