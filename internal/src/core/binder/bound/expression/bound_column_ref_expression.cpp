#include "core/binder/bound/expression/bound_column_ref_expression.hpp"

namespace litedb::core::binder::bound
{

BoundColumnRefExpression::BoundColumnRefExpression(
    common::ColumnId column_id,
    std::size_t column_ordinal,
    common::LogicalType type
)
    : BoundExpression(BoundExpressionKind::ColumnRef, type)
    , column_id_(column_id)
    , column_ordinal_(column_ordinal)
{}

common::ColumnId BoundColumnRefExpression::column_id() const noexcept
{
    return column_id_;
}

std::size_t BoundColumnRefExpression::column_ordinal() const noexcept
{
    return column_ordinal_;
}

} // namespace litedb::core::binder::bound
