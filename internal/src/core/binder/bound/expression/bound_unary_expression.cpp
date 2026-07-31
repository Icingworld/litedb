#include "core/binder/bound/expression/bound_unary_expression.hpp"

#include <memory>
#include <utility>

namespace litedb::core::binder::bound
{

BoundUnaryExpression::BoundUnaryExpression(
    common::UnaryOperator op,
    std::unique_ptr<BoundExpression> operand,
    common::LogicalType type
)
    : BoundExpression(BoundExpressionKind::Unary, type)
    , op_(op)
    , operand_(std::move(operand))
{
}

common::UnaryOperator BoundUnaryExpression::op() const noexcept
{
    return op_;
}

const BoundExpression & BoundUnaryExpression::operand() const noexcept
{
    return *operand_;
}

} // namespace litedb::core::binder::bound
