#include "core/binder/bound/expression/bound_literal_expression.hpp"

#include <utility>

namespace litedb::core::binder::bound
{

BoundLiteralExpression::BoundLiteralExpression(
    common::LogicalType type,
    std::string value,
    parser::ast::AstNodeLocation location
)
    : BoundExpression(BoundExpressionKind::Literal, type, location)
    , value_(std::move(value))
{
}

const std::string & BoundLiteralExpression::value() const noexcept
{
    return value_;
}

void BoundLiteralExpression::accept(BoundExpressionVisitor & visitor) const
{
    visitor.visit(*this);
}

} // namespace litedb::core::binder::bound
