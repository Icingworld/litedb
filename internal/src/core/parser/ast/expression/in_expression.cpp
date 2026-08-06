#include "core/parser/ast/expression/in_expression.hpp"

#include <utility>

namespace litedb::core::parser::ast
{

InExpression::InExpression(
    std::unique_ptr<ExpressionNode> expression,
    ValueList values,
    AstNodeLocation location
) noexcept
    : ExpressionNode(location)
    , expression_(std::move(expression))
    , values_(std::move(values))
{
}

AstNodeKind InExpression::kind() const noexcept
{
    return AstNodeKind::In;
}

const ExpressionNode & InExpression::expression() const noexcept
{
    return *expression_;
}

const InExpression::ValueList & InExpression::values() const noexcept
{
    return values_;
}

} // namespace litedb::core::parser::ast
