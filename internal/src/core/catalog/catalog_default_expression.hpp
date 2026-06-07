#pragma once

#include <string>
#include <vector>

namespace litedb::core::catalog
{

enum class CatalogDefaultExpressionKind
{
    Literal,
    Vector,
};

enum class CatalogDefaultLiteralKind
{
    Null,
    Boolean,
    Integer,
    Float,
    String,
};

struct CatalogDefaultExpression
{
    CatalogDefaultExpressionKind kind {CatalogDefaultExpressionKind::Literal};
    CatalogDefaultLiteralKind literal_kind {CatalogDefaultLiteralKind::Null};
    std::string value;
    std::vector<CatalogDefaultExpression> elements;

    [[nodiscard]]
    static CatalogDefaultExpression null_literal();

    [[nodiscard]]
    static CatalogDefaultExpression literal(CatalogDefaultLiteralKind literal_kind, std::string value);

    [[nodiscard]]
    static CatalogDefaultExpression vector(std::vector<CatalogDefaultExpression> elements);
};

} // namespace litedb::core::catalog
