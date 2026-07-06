#include "core/binder/bound/expression/bound_in_expression.hpp"

#include <memory>
#include <utility>

namespace litedb::core::binder::bound
{

BoundInExpression::BoundInExpression(
    std::unique_ptr<BoundExpression> expression,
    std::vector<std::unique_ptr<BoundExpression>> values,
    parser::ast::AstNodeLocation location
)
    : BoundExpression(BoundExpressionKind::In, common::LogicalType {common::LogicalTypeId::Boolean, std::nullopt}, location)
    , expression_(std::move(expression))
    , values_(std::move(values))
{
}

const BoundExpression & BoundInExpression::expression() const noexcept
{
    return *expression_;
}

const std::vector<std::unique_ptr<BoundExpression>> & BoundInExpression::values() const noexcept
{
    return values_;
}

void BoundInExpression::accept(BoundExpressionVisitor & visitor) const
{
    visitor.visit(*this);
}

std::unique_ptr<BoundExpression> BoundInExpression::clone() const
{
    std::vector<std::unique_ptr<BoundExpression>> values;
    values.reserve(values_.size());
    for (const auto & value : values_) {
        values.push_back(value->clone());
    }
    return std::make_unique<BoundInExpression>(expression_->clone(), std::move(values), location());
}

} // namespace litedb::core::binder::bound
