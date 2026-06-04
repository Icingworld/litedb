#pragma once

#include <memory>

#include "core/parser/ast/expression/expression_node.hpp"

namespace litedb::core::parser::ast
{

class BetweenExpression final : public ExpressionNode
{
public:
    BetweenExpression(
        std::unique_ptr<ExpressionNode> expression,
        std::unique_ptr<ExpressionNode> lower,
        std::unique_ptr<ExpressionNode> upper,
        AstNodeLocation location
    ) noexcept;

    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    [[nodiscard]]
    const ExpressionNode & expression() const noexcept;

    [[nodiscard]]
    const ExpressionNode & lower() const noexcept;

    [[nodiscard]]
    const ExpressionNode & upper() const noexcept;

private:
    std::unique_ptr<ExpressionNode> expression_;
    std::unique_ptr<ExpressionNode> lower_;
    std::unique_ptr<ExpressionNode> upper_;
};

} // namespace litedb::core::parser::ast
