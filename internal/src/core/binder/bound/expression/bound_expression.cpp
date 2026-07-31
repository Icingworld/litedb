#include "core/binder/bound/expression/bound_expression.hpp"

namespace litedb::core::binder::bound
{

BoundExpression::BoundExpression(
    BoundExpressionKind kind,
    common::LogicalType type
) noexcept
    : kind_(kind)
    , type_(type)
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

} // namespace litedb::core::binder::bound
