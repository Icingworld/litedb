#include "core/binder/bound/expression/bound_binary_expression.hpp"

#include <utility>

namespace litedb::core::binder::bound
{

BoundBinaryExpression::BoundBinaryExpression(
    std::unique_ptr<BoundExpression> left,
    parser::TokenType op,
    std::unique_ptr<BoundExpression> right,
    common::LogicalType type,
    parser::ast::AstNodeLocation location
)
    : BoundExpression(BoundExpressionKind::Binary, type, location),
      left_(std::move(left)),
      op_(op),
      right_(std::move(right))
{
}

const BoundExpression & BoundBinaryExpression::left() const noexcept { return *left_; }
parser::TokenType BoundBinaryExpression::op() const noexcept { return op_; }
const BoundExpression & BoundBinaryExpression::right() const noexcept { return *right_; }

} // namespace litedb::core::binder::bound
