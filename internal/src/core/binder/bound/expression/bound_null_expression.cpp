#include "core/binder/bound/expression/bound_null_expression.hpp"

namespace litedb::core::binder::bound
{

BoundNullExpression::BoundNullExpression(common::LogicalType type)
    : BoundExpression(BoundExpressionKind::Null, type)
{
}

} // namespace litedb::core::binder::bound
