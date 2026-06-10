#include "core/parser/ast/expression/identifier_expression.hpp"

#include <utility>

namespace litedb::core::parser::ast
{

IdentifierExpression::IdentifierExpression(std::string name, AstNodeLocation location) noexcept
    : ExpressionNode(location)
    , name_(std::move(name))
{
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
