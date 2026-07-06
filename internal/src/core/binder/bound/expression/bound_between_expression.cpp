#include "core/binder/bound/expression/bound_between_expression.hpp"

#include <memory>
#include <utility>

namespace litedb::core::binder::bound
{

BoundBetweenExpression::BoundBetweenExpression(
    std::unique_ptr<BoundExpression> expression,
    std::unique_ptr<BoundExpression> lower,
    std::unique_ptr<BoundExpression> upper,
    parser::ast::AstNodeLocation location
)
    : BoundExpression(BoundExpressionKind::Between, common::LogicalType {common::LogicalTypeId::Boolean, std::nullopt}, location)
    , expression_(std::move(expression))
    , lower_(std::move(lower))
    , upper_(std::move(upper))
{
}

const BoundExpression & BoundBetweenExpression::expression() const noexcept
{
    return *expression_;
}

const BoundExpression & BoundBetweenExpression::lower() const noexcept
{
    return *lower_;
}

const BoundExpression & BoundBetweenExpression::upper() const noexcept
{
    return *upper_;
}

void BoundBetweenExpression::accept(BoundExpressionVisitor & visitor) const
{
    visitor.visit(*this);
}

std::unique_ptr<BoundExpression> BoundBetweenExpression::clone() const
{
    return std::make_unique<BoundBetweenExpression>(
        expression_->clone(),
        lower_->clone(),
        upper_->clone(),
        location()
    );
}

} // namespace litedb::core::binder::bound
