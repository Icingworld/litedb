#include "core/binder/bound/statement/bound_drop_collection_statement.hpp"

namespace litedb::core::binder::bound
{

BoundDropCollectionStatement::BoundDropCollectionStatement(
    std::optional<common::CollectionId> collection_id
) noexcept
    : BoundStatement(BoundStatementKind::DropCollection)
    , collection_id_(collection_id)
{
}

std::optional<common::CollectionId>
BoundDropCollectionStatement::collection_id() const noexcept
{
    return collection_id_;
}

} // namespace litedb::core::binder::bound
