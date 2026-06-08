#include "core/binder/bound/statement/bound_drop_collection_statement.hpp"

#include <utility>

namespace litedb::core::binder::bound
{

BoundDropCollectionStatement::BoundDropCollectionStatement(
    common::DatabaseId database_id,
    std::optional<common::CollectionId> collection_id,
    std::string collection_name,
    bool if_exists,
    parser::ast::AstNodeLocation location
)
    : BoundStatement(BoundStatementKind::DropCollection, location),
      database_id_(database_id),
      collection_id_(collection_id),
      collection_name_(std::move(collection_name)),
      if_exists_(if_exists)
{
}

common::DatabaseId BoundDropCollectionStatement::database_id() const noexcept { return database_id_; }
std::optional<common::CollectionId> BoundDropCollectionStatement::collection_id() const noexcept { return collection_id_; }
const std::string & BoundDropCollectionStatement::collection_name() const noexcept { return collection_name_; }
bool BoundDropCollectionStatement::if_exists() const noexcept { return if_exists_; }

} // namespace litedb::core::binder::bound
