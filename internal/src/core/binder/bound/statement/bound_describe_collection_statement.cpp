#include "core/binder/bound/statement/bound_describe_collection_statement.hpp"

namespace litedb::core::binder::bound
{

BoundDescribeCollectionStatement::BoundDescribeCollectionStatement(
    common::CollectionId collection_id
) noexcept
    : BoundStatement(BoundStatementKind::DescribeCollection)
    , collection_id_(collection_id)
{
}

common::CollectionId
BoundDescribeCollectionStatement::collection_id() const noexcept
{
    return collection_id_;
}

} // namespace litedb::core::binder::bound
