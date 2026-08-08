#pragma once

#include <optional>
#include <string>

#include "core/parser/ast/expression/expression_node.hpp"

namespace litedb::core::parser::ast
{

// 列引用表达式节点
// 示例：qualifier.column
class ColumnReferenceExpression final : public ExpressionNode
{
public:
    ColumnReferenceExpression(
        std::optional<std::string> qualifier,
        std::string column_name,
        AstNodeLocation location
    );

public:
    // 获取节点类型
    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    // 获取限定符
    [[nodiscard]]
    const std::optional<std::string> & qualifier() const noexcept;

    // 获取列名
    [[nodiscard]]
    const std::string & column_name() const noexcept;

private:
    std::optional<std::string> qualifier_;    // 限定符
    std::string column_name_;                 // 列名
};

} // namespace litedb::core::parser::ast
