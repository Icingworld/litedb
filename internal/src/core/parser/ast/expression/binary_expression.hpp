#pragma once

#include <memory>

#include "core/parser/ast/expression/expression_node.hpp"
#include "core/parser/token.hpp"

namespace litedb::core::parser::ast
{

class BinaryExpression final : public ExpressionNode
{
public:
    BinaryExpression(
        std::unique_ptr<ExpressionNode> left,
        TokenType op,
        std::unique_ptr<ExpressionNode> right,
        AstNodeLocation location
    ) noexcept;

    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    [[nodiscard]]
    const ExpressionNode & left() const noexcept;

    [[nodiscard]]
    TokenType op() const noexcept;

    [[nodiscard]]
    const ExpressionNode & right() const noexcept;

private:
    std::unique_ptr<ExpressionNode> left_;
    TokenType op_;
    std::unique_ptr<ExpressionNode> right_;
};

} // namespace litedb::core::parser::ast
