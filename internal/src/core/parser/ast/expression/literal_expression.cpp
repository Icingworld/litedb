#include "core/parser/ast/expression/literal_expression.hpp"

#include <utility>

namespace litedb::core::parser::ast
{

LiteralExpression::LiteralExpression(TokenType literal_type, std::string value, AstNodeLocation location) noexcept
    : ExpressionNode(location)
    , literal_type_(literal_type)
    , value_(std::move(value))
{
}

AstNodeKind LiteralExpression::kind() const noexcept
{
    return AstNodeKind::Literal;
}

TokenType LiteralExpression::literal_type() const noexcept
{
    return literal_type_;
}

const std::string & LiteralExpression::value() const noexcept
{
    return value_;
}

} // namespace litedb::core::parser::ast
