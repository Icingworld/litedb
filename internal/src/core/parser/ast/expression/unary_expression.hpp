#pragma once

#include <memory>

#include "core/parser/ast/expression/expression_node.hpp"
#include "core/parser/token.hpp"

namespace litedb::core::parser::ast
{

// 一元表达式节点
class UnaryExpression final : public ExpressionNode
{
public:
    UnaryExpression(
        TokenType op,
        std::unique_ptr<ExpressionNode> operand,
        AstNodeLocation location
    );

public:
    // 获取节点类型
    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    // 获取操作符
    [[nodiscard]]
    TokenType op() const noexcept;

    // 获取操作数
    [[nodiscard]]
    const ExpressionNode & operand() const noexcept;

private:
    TokenType op_;
    std::unique_ptr<ExpressionNode> operand_;
};

} // namespace litedb::core::parser::ast
