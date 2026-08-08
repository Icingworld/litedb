#include "core/parser/ast/expression/literal_expression.hpp"

#include <cassert>
#include <utility>

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
    // 不验证 value 是否为空，Lexer 可以合法产出空字符串
    // 比如：COMMENT "", 或者：VALUES ("", 1) 等
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
