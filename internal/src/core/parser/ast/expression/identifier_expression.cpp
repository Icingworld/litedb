#include "core/parser/ast/expression/identifier_expression.hpp"

#include <utility>
#include <cassert>

namespace litedb::core::parser::ast
{

IdentifierExpression::IdentifierExpression(
    std::string name,
    AstNodeLocation location
)
    : ExpressionNode(location)
    , name_(std::move(name))
{
    assert(!name_.empty());
}

AstNodeKind IdentifierExpression::kind() const noexcept
{
    return AstNodeKind::Identifier;
}

const std::string & IdentifierExpression::name() const noexcept
{
    return name_;
}

} // namespace litedb::core::parser::ast
