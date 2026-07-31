#include "core/binder/bound/expression/bound_vector_expression.hpp"

#include <utility>

namespace litedb::core::binder::bound
{

BoundVectorExpression::BoundVectorExpression(
    std::vector<std::unique_ptr<BoundExpression>> elements
)
    : BoundExpression(
        BoundExpressionKind::Vector,
        common::LogicalType {
            common::LogicalTypeId::Vector,
            elements.size()
        }
    )
    , elements_(std::move(elements))
{
}

const std::vector<std::unique_ptr<BoundExpression>> &
BoundVectorExpression::elements() const noexcept
{
    return elements_;
}

} // namespace litedb::core::binder::bound
