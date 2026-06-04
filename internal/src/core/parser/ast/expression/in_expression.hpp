#pragma once

#include <memory>
#include <vector>

#include "core/parser/ast/expression/expression_node.hpp"

namespace litedb::core::parser::ast
{

class InExpression final : public ExpressionNode
{
public:
    using ValueList = std::vector<std::unique_ptr<ExpressionNode>>;

    InExpression(std::unique_ptr<ExpressionNode> expression, ValueList values, AstNodeLocation location) noexcept;

    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    [[nodiscard]]
    const ExpressionNode & expression() const noexcept;

    [[nodiscard]]
    const ValueList & values() const noexcept;

private:
    std::unique_ptr<ExpressionNode> expression_;
    ValueList values_;
};

} // namespace litedb::core::parser::ast
