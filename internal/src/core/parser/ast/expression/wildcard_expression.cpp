#include "core/parser/ast/expression/wildcard_expression.hpp"

#include <utility>

namespace litedb::core::parser::ast
{

WildcardExpression::WildcardExpression(AstNodeLocation location) noexcept
    : ExpressionNode(location)
{
}

WildcardExpression::WildcardExpression(
    std::optional<std::string> qualifier,
    AstNodeLocation location
) noexcept
    : ExpressionNode(location)
    , qualifier_(std::move(qualifier))
{
}

AstNodeKind WildcardExpression::kind() const noexcept
{
    return AstNodeKind::Wildcard;
}

const std::optional<std::string> & WildcardExpression::qualifier() const noexcept
{
    return qualifier_;
}

} // namespace litedb::core::parser::ast
