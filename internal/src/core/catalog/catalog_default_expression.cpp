#include "core/catalog/catalog_default_expression.hpp"

#include <utility>

namespace litedb::core::catalog
{

CatalogDefaultExpression CatalogDefaultExpression::null_literal()
{
    return {};
}

CatalogDefaultExpression CatalogDefaultExpression::literal(
    CatalogDefaultLiteralKind literal_kind,
    std::string value
)
{
    CatalogDefaultExpression expression;
    expression.kind = CatalogDefaultExpressionKind::Literal;
    expression.literal_kind = literal_kind;
    expression.value = std::move(value);
    return expression;
}

CatalogDefaultExpression CatalogDefaultExpression::vector(std::vector<CatalogDefaultExpression> elements)
{
    CatalogDefaultExpression expression;
    expression.kind = CatalogDefaultExpressionKind::Vector;
    expression.elements = std::move(elements);
    return expression;
}

} // namespace litedb::core::catalog
