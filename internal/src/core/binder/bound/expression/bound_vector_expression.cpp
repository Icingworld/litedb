#include "core/binder/bound/expression/bound_vector_expression.hpp"

#include <memory>
#include <utility>

namespace litedb::core::binder::bound
{

BoundVectorExpression::BoundVectorExpression(
    std::vector<std::unique_ptr<BoundExpression>> elements,
    common::LogicalType type,
    parser::ast::AstNodeLocation location
)
    : BoundExpression(BoundExpressionKind::Vector, type, location)
    , elements_(std::move(elements))
{
}

const std::vector<std::unique_ptr<BoundExpression>> & BoundVectorExpression::elements() const noexcept
{
    return elements_;
}

void BoundVectorExpression::accept(BoundExpressionVisitor & visitor) const
{
    visitor.visit(*this);
}

std::unique_ptr<BoundExpression> BoundVectorExpression::clone() const
{
    std::vector<std::unique_ptr<BoundExpression>> elements;
    elements.reserve(elements_.size());
    for (const auto & element : elements_) {
        elements.push_back(element->clone());
    }
    return std::make_unique<BoundVectorExpression>(std::move(elements), type(), location());
}

} // namespace litedb::core::binder::bound
