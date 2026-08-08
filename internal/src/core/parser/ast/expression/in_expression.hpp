#pragma once

#include <memory>
#include <vector>

#include "core/parser/ast/expression/expression_node.hpp"

namespace litedb::core::parser::ast
{

// IN 表达式节点
class InExpression final : public ExpressionNode
{
public:
    InExpression(
        std::unique_ptr<ExpressionNode> expression,
        std::vector<std::unique_ptr<ExpressionNode>> values,
        AstNodeLocation location
    );

public:
    // 获取节点类型
    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    // 获取表达式
    [[nodiscard]]
    const ExpressionNode & expression() const noexcept;

    // 获取值列表
    [[nodiscard]]
    const std::vector<std::unique_ptr<ExpressionNode>> & values() const noexcept;

private:
    std::unique_ptr<ExpressionNode> expression_;            // 表达式
    std::vector<std::unique_ptr<ExpressionNode>> values_;   // 值列表
};

} // namespace litedb::core::parser::ast
