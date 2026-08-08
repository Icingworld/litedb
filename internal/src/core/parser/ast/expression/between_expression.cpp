#include "core/parser/ast/expression/between_expression.hpp"

#include <cassert>
#include <utility>

namespace litedb::core::parser::ast
{

BetweenExpression::BetweenExpression(
    std::unique_ptr<ExpressionNode> expression,
    std::unique_ptr<ExpressionNode> lower,
    std::unique_ptr<ExpressionNode> upper,
    AstNodeLocation location
)
    : ExpressionNode(location)
    , expression_(std::move(expression))
    , lower_(std::move(lower))
    , upper_(std::move(upper))
{
    assert(expression_ != nullptr);
    assert(lower_ != nullptr);
    assert(upper_ != nullptr);
}

AstNodeKind BetweenExpression::kind() const noexcept
{
    return AstNodeKind::Between;
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
