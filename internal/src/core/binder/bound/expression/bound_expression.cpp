#include "core/binder/bound/expression/bound_expression.hpp"

namespace litedb::core::binder::bound
{

BoundExpression::BoundExpression(
    BoundExpressionKind kind,
    common::LogicalType type,
    parser::ast::AstNodeLocation location
) noexcept
    : kind_(kind),
      type_(type),
      location_(location)
{
}

BoundExpressionKind BoundExpression::kind() const noexcept
{
    return kind_;
}

const common::LogicalType & BoundExpression::type() const noexcept
{
    return type_;
}

parser::ast::AstNodeLocation BoundExpression::location() const noexcept
{
    return location_;
}

} // namespace litedb::core::binder::bound
