#include "core/parser/ast/expression/alias_expression.hpp"

#include <utility>

namespace litedb::core::parser::ast
{

AliasExpression::AliasExpression(
    std::unique_ptr<ExpressionNode> expression,
    std::string alias,
    AstNodeLocation location
) noexcept
    : ExpressionNode(location)
    , expression_(std::move(expression))
    , alias_(std::move(alias))
{
}

AstNodeKind AliasExpression::kind() const noexcept
{
    return AstNodeKind::Alias;
}

const ExpressionNode & AliasExpression::expression() const noexcept
{
    return *expression_;
}

const std::string & AliasExpression::alias() const noexcept
{
    return alias_;
}

} // namespace litedb::core::parser::ast
