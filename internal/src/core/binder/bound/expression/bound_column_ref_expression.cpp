#include "core/binder/bound/expression/bound_column_ref_expression.hpp"

#include <utility>

namespace litedb::core::binder::bound
{

BoundColumnRefExpression::BoundColumnRefExpression(
    common::DatabaseId database_id,
    common::CollectionId collection_id,
    std::string collection_name,
    common::ColumnId column_id,
    std::string column_name,
    common::LogicalType type,
    bool nullable,
    parser::ast::AstNodeLocation location
)
    : BoundExpression(BoundExpressionKind::ColumnRef, type, location)
    , database_id_(database_id)
    , collection_id_(collection_id)
    , collection_name_(std::move(collection_name))
    , column_id_(column_id)
    , column_name_(std::move(column_name))
    , nullable_(nullable)
{
}

common::DatabaseId BoundColumnRefExpression::database_id() const noexcept
{
    return database_id_;
}

common::CollectionId BoundColumnRefExpression::collection_id() const noexcept
{
    return collection_id_;
}

const std::string & BoundColumnRefExpression::collection_name() const noexcept
{
    return collection_name_;
}

common::ColumnId BoundColumnRefExpression::column_id() const noexcept
{
    return column_id_;
}

const std::string & BoundColumnRefExpression::column_name() const noexcept
{
    return column_name_;
}

bool BoundColumnRefExpression::nullable() const noexcept
{
    return nullable_;
}

void BoundColumnRefExpression::accept(BoundExpressionVisitor & visitor) const
{
    visitor.visit(*this);
}

} // namespace litedb::core::binder::bound
