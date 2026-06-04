#pragma once

#include <memory>

#include "core/parser/ast/expression/expression_node.hpp"
#include "core/parser/token.hpp"

namespace litedb::core::parser::ast
{

class UnaryExpression final : public ExpressionNode
{
public:
    UnaryExpression(TokenType op, std::unique_ptr<ExpressionNode> operand, AstNodeLocation location) noexcept;

    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    [[nodiscard]]
    TokenType op() const noexcept;

    [[nodiscard]]
    const ExpressionNode & operand() const noexcept;

private:
    TokenType op_;
    std::unique_ptr<ExpressionNode> operand_;
};

} // namespace litedb::core::parser::ast
