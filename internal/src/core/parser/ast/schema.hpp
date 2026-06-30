#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/parser/ast/expression/expression_node.hpp"

namespace litedb::core::parser::ast
{

/**
 * @brief schema 对象类型
 */
enum class SchemaObjectType
{
    Database,
    Collection,
};

/**
 * @brief 字段数据类型
 */
enum class DataTypeKind
{
    Integer,
    BigInt,
    Float,
    Double,
    Varchar,
    Boolean,
    Vector,
};

/**
 * @brief 字段数据类型描述
 * @note 如 VARCHAR(128) / VECTOR(128)
 */
struct DataType
{
    DataTypeKind kind;                        ///< 数据类型
    std::optional<std::size_t> parameter;     ///< 参数
};

/**
 * @brief 字段定义
 */
struct ColumnDefinition
{
    std::string name;                                 ///< 列名称
    DataType type;                                    ///< 数据类型
    bool unique {false};                              ///< 是否唯一
    std::unique_ptr<ExpressionNode> default_value;    ///< 默认值
    std::optional<std::string> comment;               ///< 注释
};

/**
 * @brief 列定义列表
 */
using ColumnDefinitionList = std::vector<ColumnDefinition>;

} // namespace litedb::core::parser::ast
