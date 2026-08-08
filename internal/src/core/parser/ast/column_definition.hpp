#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/common/logical_type.hpp"
#include "core/parser/ast/expression/expression_node.hpp"

namespace litedb::core::parser::ast
{

// CREATE COLLECTION 中尚未绑定的列定义
struct ColumnDefinitionSyntax
{
    std::string name;                                 // 列名称
    common::LogicalType type {};                      // 逻辑类型
    bool unique {false};                              // 是否唯一
    bool nullable {true};                             // 是否可为 NULL
    std::unique_ptr<ExpressionNode> default_value;    // 默认值语法树
    std::optional<std::string> comment;               // 注释
    AstNodeLocation location {};                      // 列定义位置
};

using ColumnDefinitionSyntaxList = std::vector<std::unique_ptr<ColumnDefinitionSyntax>>;

} // namespace litedb::core::parser::ast
