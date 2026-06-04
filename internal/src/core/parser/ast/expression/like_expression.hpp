#pragma once

#include <memory>

#include "core/parser/ast/expression/expression_node.hpp"

namespace litedb::core::parser::ast
{

class LikeExpression final : public ExpressionNode
{
public:
    LikeExpression(
        std::unique_ptr<ExpressionNode> expression,
        std::unique_ptr<ExpressionNode> pattern,
        AstNodeLocation location
    ) noexcept;

    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    [[nodiscard]]
    const ExpressionNode & expression() const noexcept;

    [[nodiscard]]
    const ExpressionNode & pattern() const noexcept;

private:
    std::unique_ptr<ExpressionNode> expression_;
    std::unique_ptr<ExpressionNode> pattern_;
};

} // namespace litedb::core::parser::ast
