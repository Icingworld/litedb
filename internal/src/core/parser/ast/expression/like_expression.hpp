#pragma once

#include <memory>

#include "core/parser/ast/expression/expression_node.hpp"

namespace litedb::core::parser::ast
{

// LIKE 表达式节点
class LikeExpression final : public ExpressionNode
{
public:
    LikeExpression(
        std::unique_ptr<ExpressionNode> expression,
        std::unique_ptr<ExpressionNode> pattern,
        AstNodeLocation location
    );

public:
    // 获取节点类型
    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    // 获取表达式
    [[nodiscard]]
    const ExpressionNode & expression() const noexcept;

    // 获取模式
    [[nodiscard]]
    const ExpressionNode & pattern() const noexcept;

private:
    std::unique_ptr<ExpressionNode> expression_;
    std::unique_ptr<ExpressionNode> pattern_;
};

} // namespace litedb::core::parser::ast
