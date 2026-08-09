#include "core/binder/bound/statement/bound_show_indexes_statement.hpp"

namespace litedb::core::binder::bound
{

BoundShowIndexesStatement::BoundShowIndexesStatement(common::CollectionId collection_id) noexcept
    : BoundStatement(BoundStatementKind::ShowIndexes)
    , collection_id_(collection_id)
{}

common::CollectionId BoundShowIndexesStatement::collection_id() const noexcept
{
    return collection_id_;
}

} // namespace litedb::core::binder::bound
