#include "core/binder/bound/expression/bound_literal_expression.hpp"

#include <utility>

namespace litedb::core::binder::bound
{

BoundLiteralExpression::BoundLiteralExpression(
    common::LogicalType type,
    common::Value value
)
    : BoundExpression(BoundExpressionKind::Literal, type)
    , value_(std::move(value))
{
}

const common::Value & BoundLiteralExpression::value() const noexcept
{
    return value_;
}

} // namespace litedb::core::binder::bound
