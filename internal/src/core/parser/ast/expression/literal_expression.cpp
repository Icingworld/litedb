#include "core/parser/ast/expression/literal_expression.hpp"

#include <utility>
#include <cassert>

namespace litedb::core::parser::ast
{

LiteralExpression::LiteralExpression(
    TokenType literal_type,
    std::string value,
    AstNodeLocation location
)
    : ExpressionNode(location)
    , literal_type_(literal_type)
    , value_(std::move(value))
{
    assert(literal_type_ == TokenType::Null || !value_.empty());
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
