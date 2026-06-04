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

const VectorExpression::ElementList & VectorExpression::elements() const noexcept
{
    return elements_;
}

} // namespace litedb::core::parser::ast
