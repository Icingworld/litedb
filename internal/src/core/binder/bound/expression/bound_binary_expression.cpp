#include "core/binder/bound/expression/bound_binary_expression.hpp"

#include <utility>

namespace litedb::core::binder::bound
{

BoundBinaryExpression::BoundBinaryExpression(
    std::unique_ptr<BoundExpression> left,
    common::BinaryOperator op,
    std::unique_ptr<BoundExpression> right,
    common::LogicalType type
)
    : BoundExpression(BoundExpressionKind::Binary, type)
    , left_(std::move(left))
    , op_(op)
    , right_(std::move(right))
{}

const BoundExpression & BoundBinaryExpression::left() const noexcept
{
    return *left_;
}

std::unique_ptr<BoundExpression> BoundBinaryExpression::take_left() noexcept
{
    return std::exchange(left_, nullptr);
}

common::BinaryOperator BoundBinaryExpression::op() const noexcept
{
    return op_;
}

const BoundExpression & BoundBinaryExpression::right() const noexcept
{
    return *right_;
}

std::unique_ptr<BoundExpression> BoundBinaryExpression::take_right() noexcept
{
    return std::exchange(right_, nullptr);
}

} // namespace litedb::core::binder::bound
