#include "core/binder/bound/expression/bound_wildcard_expression.hpp"

#include <memory>
#include <utility>

namespace litedb::core::binder::bound
{

BoundWildcardExpression::BoundWildcardExpression(
    std::optional<std::string> qualifier,
    parser::ast::AstNodeLocation location
)
    : BoundExpression(BoundExpressionKind::Wildcard, common::LogicalType {common::LogicalTypeId::Null, std::nullopt}, location)
    , qualifier_(std::move(qualifier))
{
}

const std::optional<std::string> & BoundWildcardExpression::qualifier() const noexcept
{
    return qualifier_;
}

void BoundWildcardExpression::accept(BoundExpressionVisitor & visitor) const
{
    visitor.visit(*this);
}

std::unique_ptr<BoundExpression> BoundWildcardExpression::clone() const
{
    return std::make_unique<BoundWildcardExpression>(qualifier_, location());
}

} // namespace litedb::core::binder::bound
