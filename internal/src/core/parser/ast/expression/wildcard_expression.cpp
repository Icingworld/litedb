#include "core/parser/ast/expression/wildcard_expression.hpp"

#include <cassert>
#include <utility>

namespace litedb::core::parser::ast
{

WildcardExpression::WildcardExpression(AstNodeLocation location) noexcept
    : ExpressionNode(location)
    , qualifier_(std::nullopt)
{}

WildcardExpression::WildcardExpression(
    std::optional<std::string> qualifier,
    AstNodeLocation location
)
    : ExpressionNode(location)
    , qualifier_(std::move(qualifier))
{
    if (qualifier_.has_value()) {
        assert(!qualifier_.value().empty());
    }
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
