#pragma once

#include <memory>

#include "core/parser/ast/expression/expression_node.hpp"
#include "core/parser/token.hpp"

namespace litedb::core::parser::ast
{

// 二元表达式节点
class BinaryExpression final : public ExpressionNode
{
public:
    BinaryExpression(
        std::unique_ptr<ExpressionNode> left,
        TokenType op,
        std::unique_ptr<ExpressionNode> right,
        AstNodeLocation location
    );

public:
    // 获取节点类型
    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    // 获取左操作数
    [[nodiscard]]
    const ExpressionNode & left() const noexcept;

    // 获取操作符
    [[nodiscard]]
    TokenType op() const noexcept;

    // 获取右操作数
    [[nodiscard]]
    const ExpressionNode & right() const noexcept;

private:
    std::unique_ptr<ExpressionNode> left_;
    TokenType op_;
    std::unique_ptr<ExpressionNode> right_;
};

} // namespace litedb::core::parser::ast
