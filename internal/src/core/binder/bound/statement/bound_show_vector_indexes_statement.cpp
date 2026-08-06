#include "core/binder/bound/statement/bound_show_vector_indexes_statement.hpp"

namespace litedb::core::binder::bound
{

BoundShowVectorIndexesStatement::BoundShowVectorIndexesStatement(
    common::CollectionId collection_id
) noexcept
    : BoundStatement(BoundStatementKind::ShowVectorIndexes)
    , collection_id_(collection_id)
{
}

common::CollectionId
BoundShowVectorIndexesStatement::collection_id() const noexcept
{
    return collection_id_;
}

} // namespace litedb::core::binder::bound
