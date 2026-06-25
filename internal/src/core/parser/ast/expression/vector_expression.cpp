#include "core/parser/ast/expression/vector_expression.hpp"

#include <utility>

namespace litedb::core::parser::ast
{

VectorExpression::VectorExpression(ElementList elements, AstNodeLocation location) noexcept
    : ExpressionNode(location)
    , elements_(std::move(elements))
{
}

AstNodeKind VectorExpression::kind() const noexcept
{
    return AstNodeKind::Vector;
}

void VectorExpression::accept(AstNodeVisitor & visitor) const
{
    visitor.visit(*this);
}

const VectorExpression::ElementList & VectorExpression::elements() const noexcept
{
    return elements_;
}

} // namespace litedb::core::parser::ast
