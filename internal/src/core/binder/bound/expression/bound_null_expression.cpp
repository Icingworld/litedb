#include "core/binder/bound/expression/bound_null_expression.hpp"

namespace litedb::core::binder::bound
{

BoundNullExpression::BoundNullExpression(common::LogicalType type, parser::ast::AstNodeLocation location)
    : BoundExpression(BoundExpressionKind::Null, type, location)
{
}

} // namespace litedb::core::binder::bound
