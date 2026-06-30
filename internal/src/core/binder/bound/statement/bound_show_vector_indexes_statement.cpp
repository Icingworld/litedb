#include "core/binder/bound/statement/bound_show_vector_indexes_statement.hpp"

#include <utility>

namespace litedb::core::binder::bound
{

BoundShowVectorIndexesStatement::BoundShowVectorIndexesStatement(
    common::DatabaseId database_id,
    common::CollectionId collection_id,
    std::string collection_name,
    parser::ast::AstNodeLocation location
)
    : BoundStatement(BoundStatementKind::ShowVectorIndexes, location)
    , database_id_(database_id)
    , collection_id_(collection_id)
    , collection_name_(std::move(collection_name))
{
}

common::DatabaseId BoundShowVectorIndexesStatement::database_id() const noexcept
{
    return database_id_;
}

common::CollectionId BoundShowVectorIndexesStatement::collection_id() const noexcept
{
    return collection_id_;
}

const std::string & BoundShowVectorIndexesStatement::collection_name() const noexcept
{
    return collection_name_;
}

void BoundShowVectorIndexesStatement::accept(BoundStatementVisitor & visitor) const
{
    visitor.visit(*this);
}

} // namespace litedb::core::binder::bound
