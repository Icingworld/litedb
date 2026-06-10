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
    DataTypeKind kind;
    std::optional<std::size_t> parameter;
};

/**
 * @brief 字段定义
 */
struct ColumnDefinition
{
    std::string name;
    DataType type;
    bool primary_key {false};
    bool unique {false};
    std::unique_ptr<ExpressionNode> default_value;
    std::optional<std::string> comment;
};

using ColumnDefinitionList = std::vector<ColumnDefinition>;

} // namespace litedb::core::parser::ast
