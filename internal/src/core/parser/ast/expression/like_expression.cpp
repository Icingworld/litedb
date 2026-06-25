#include "core/parser/ast/expression/like_expression.hpp"

#include <utility>

namespace litedb::core::parser::ast
{

LikeExpression::LikeExpression(
    std::unique_ptr<ExpressionNode> expression,
    std::unique_ptr<ExpressionNode> pattern,
    AstNodeLocation location
) noexcept
    : ExpressionNode(location)
    , expression_(std::move(expression))
    , pattern_(std::move(pattern))
{
}

AstNodeKind LikeExpression::kind() const noexcept
{
    return AstNodeKind::Like;
}

void LikeExpression::accept(AstNodeVisitor & visitor) const
{
    visitor.visit(*this);
}

const ExpressionNode & LikeExpression::expression() const noexcept
{
    return *expression_;
}

const ExpressionNode & LikeExpression::pattern() const noexcept
{
    return *pattern_;
}

} // namespace litedb::core::parser::ast
