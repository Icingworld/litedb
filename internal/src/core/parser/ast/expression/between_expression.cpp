#include "core/parser/ast/expression/between_expression.hpp"

#include <utility>

namespace litedb::core::parser::ast
{

BetweenExpression::BetweenExpression(
    std::unique_ptr<ExpressionNode> expression,
    std::unique_ptr<ExpressionNode> lower,
    std::unique_ptr<ExpressionNode> upper,
    AstNodeLocation location
) noexcept
    : ExpressionNode(location)
    , expression_(std::move(expression))
    , lower_(std::move(lower))
    , upper_(std::move(upper))
{
}

AstNodeKind BetweenExpression::kind() const noexcept
{
    return AstNodeKind::Between;
}

void BetweenExpression::accept(AstNodeVisitor & visitor) const
{
    visitor.visit(*this);
}

const ExpressionNode & BetweenExpression::expression() const noexcept
{
    return *expression_;
}

const ExpressionNode & BetweenExpression::lower() const noexcept
{
    return *lower_;
}

const ExpressionNode & BetweenExpression::upper() const noexcept
{
    return *upper_;
}

} // namespace litedb::core::parser::ast
