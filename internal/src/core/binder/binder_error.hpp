#pragma once

#include <cstdint>

#include "core/error/error.hpp"
#include "core/parser/ast/ast_node.hpp"

namespace litedb::core::binder
{

/**
 * @brief 绑定错误码
 */
enum class BinderErrorCode : std::uint8_t
{
    UnsupportedStatement = 0,      ///< 不支持的语句
    UnsupportedExpression = 1,     ///< 不支持的表达式
    DatabaseNotSelected = 2,       ///< 数据库未选择
    DatabaseNotFound = 3,          ///< 数据库不存在
    CollectionNotFound = 4,        ///< 集合不存在
    ColumnNotFound = 5,            ///< 列不存在
    IndexNotFound = 6,             ///< 索引不存在
    DatabaseAlreadyExists = 7,     ///< 数据库已存在
    CollectionAlreadyExists = 8,   ///< 集合已存在
    IndexAlreadyExists = 9,        ///< 索引已存在
    VectorIndexAlreadyExists = 10, ///< 向量索引已存在
    DuplicateColumn = 11,          ///< 列已存在
    AmbiguousAlias = 12,           ///< 别名不明确
    InvalidQualifier = 13,         ///< 无效的限定符
    InvalidType = 14,              ///< 无效的类型
    InvalidValueCount = 15,        ///< 无效的值数量
    NotNullable = 16,              ///< 不能为 NULL
    VectorIndexNotFound = 17,      ///< 向量索引不存在
    InvalidIndexOptions = 18,      ///< 无效的索引选项
    InvalidLiteral = 19,           ///< 无效的字面量
};

/**
 * @brief 绑定错误上下文
 */
struct BinderErrorContext
{
    parser::ast::AstNodeLocation location;  ///< AST 中的错误位置
};

using BinderError = error::Error;

} // namespace litedb::core::binder

namespace litedb::core::error
{

template <>
struct ErrorTraits<binder::BinderErrorCode>
{
    static constexpr ErrorCategory category = ErrorCategory::Binder;
};

} // namespace litedb::core::error
