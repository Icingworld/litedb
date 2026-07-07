#include "core/binder/bound/expression/bound_null_expression.hpp"

#include <memory>

namespace litedb::core::binder::bound
{

BoundNullExpression::BoundNullExpression(common::LogicalType type, parser::ast::AstNodeLocation location)
    : BoundExpression(BoundExpressionKind::Null, type, location)
{
}

void BoundNullExpression::accept(BoundExpressionVisitor & visitor) const
{
    visitor.visit(*this);
}

std::unique_ptr<BoundExpression> BoundNullExpression::clone() const
{
    return std::make_unique<BoundNullExpression>(type(), location());
}

} // namespace litedb::core::binder::bound
