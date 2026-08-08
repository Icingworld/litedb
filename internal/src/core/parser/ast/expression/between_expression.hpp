#pragma once

#include <memory>

#include "core/parser/ast/expression/expression_node.hpp"

namespace litedb::core::parser::ast
{

// BETWEEN 表达式节点
class BetweenExpression final : public ExpressionNode
{
public:
    BetweenExpression(
        std::unique_ptr<ExpressionNode> expression,
        std::unique_ptr<ExpressionNode> lower,
        std::unique_ptr<ExpressionNode> upper,
        AstNodeLocation location
    );

public:
    // 获取节点类型
    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    // 获取表达式
    [[nodiscard]]
    const ExpressionNode & expression() const noexcept;

    // 获取下界
    [[nodiscard]]
    const ExpressionNode & lower() const noexcept;

    // 获取上界
    [[nodiscard]]
    const ExpressionNode & upper() const noexcept;

private:
    std::unique_ptr<ExpressionNode> expression_;
    std::unique_ptr<ExpressionNode> lower_;
    std::unique_ptr<ExpressionNode> upper_;
};

} // namespace litedb::core::parser::ast
