#include "core/schema/default_expression.hpp"

#include <utility>

namespace litedb::core::schema
{

DefaultExpression DefaultExpression::null_literal()
{
    return {};
}

DefaultExpression DefaultExpression::literal(DefaultLiteralKind literal_kind, std::string value)
{
    DefaultExpression expression;
    expression.kind = DefaultExpressionKind::Literal;
    expression.literal_kind = literal_kind;
    expression.value = std::move(value);
    return expression;
}

DefaultExpression DefaultExpression::vector(std::vector<DefaultExpression> elements)
{
    DefaultExpression expression;
    expression.kind = DefaultExpressionKind::Vector;
    expression.elements = std::move(elements);
    return expression;
}

} // namespace litedb::core::schema
