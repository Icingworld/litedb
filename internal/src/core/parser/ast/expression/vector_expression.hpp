#pragma once

#include <memory>
#include <vector>

#include "core/parser/ast/expression/expression_node.hpp"

namespace litedb::core::parser::ast
{

class VectorExpression final : public ExpressionNode
{
public:
    using ElementList = std::vector<std::unique_ptr<ExpressionNode>>;

    VectorExpression(ElementList elements, AstNodeLocation location) noexcept;

    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    [[nodiscard]]
    const ElementList & elements() const noexcept;

private:
    ElementList elements_;
};

} // namespace litedb::core::parser::ast
