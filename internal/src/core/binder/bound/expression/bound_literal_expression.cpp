#include "core/binder/bound/expression/bound_literal_expression.hpp"

#include <utility>

namespace litedb::core::binder::bound
{

BoundLiteralExpression::BoundLiteralExpression(
    common::LogicalType type,
    std::string value
)
    : BoundExpression(BoundExpressionKind::Literal, type)
    , value_(std::move(value))
{
}

const std::string & BoundLiteralExpression::value() const noexcept
{
    return value_;
}

} // namespace litedb::core::binder::bound
