#include "core/binder/bound/statement/bound_delete_statement.hpp"

#include <utility>

namespace litedb::core::binder::bound
{

BoundDeleteStatement::BoundDeleteStatement(
    common::DatabaseId database_id,
    common::CollectionId collection_id,
    std::string collection_name,
    std::unique_ptr<BoundExpression> where,
    parser::ast::AstNodeLocation location
)
    : BoundStatement(BoundStatementKind::Delete, location)
    , database_id_(database_id)
    , collection_id_(collection_id)
    , collection_name_(std::move(collection_name))
    , where_(std::move(where))
{
}

common::DatabaseId BoundDeleteStatement::database_id() const noexcept
{
    return database_id_;
}

common::CollectionId BoundDeleteStatement::collection_id() const noexcept
{
    return collection_id_;
}

const std::string & BoundDeleteStatement::collection_name() const noexcept
{
    return collection_name_;
}

const BoundExpression * BoundDeleteStatement::where() const noexcept
{
    return where_.get();
}

std::unique_ptr<BoundExpression> BoundDeleteStatement::take_where() noexcept
{
    return std::move(where_);
}

void BoundDeleteStatement::accept(BoundStatementVisitor & visitor) const
{
    visitor.visit(*this);
}

} // namespace litedb::core::binder::bound
