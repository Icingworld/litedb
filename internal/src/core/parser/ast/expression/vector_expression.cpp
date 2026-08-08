#include "core/parser/ast/expression/vector_expression.hpp"

#include <cassert>
#include <utility>

namespace litedb::core::parser::ast
{

VectorExpression::VectorExpression(
    std::vector<std::unique_ptr<ExpressionNode>> elements,
    AstNodeLocation location
)
    : ExpressionNode(location)
    , elements_(std::move(elements))
{
    assert(!elements_.empty());
    for (const auto & element : elements_) {
        assert(element != nullptr);
    }
}

AstNodeKind VectorExpression::kind() const noexcept
{
    return AstNodeKind::Vector;
}

const std::vector<std::unique_ptr<ExpressionNode>> & VectorExpression::elements() const noexcept
{
    return elements_;
}

} // namespace litedb::core::parser::ast
