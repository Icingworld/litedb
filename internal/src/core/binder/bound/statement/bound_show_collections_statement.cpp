#include "core/binder/bound/statement/bound_show_collections_statement.hpp"

namespace litedb::core::binder::bound
{

BoundShowCollectionsStatement::BoundShowCollectionsStatement(
    common::DatabaseId database_id,
    parser::ast::AstNodeLocation location
)
    : BoundStatement(BoundStatementKind::ShowCollections, location),
      database_id_(database_id)
{
}

common::DatabaseId BoundShowCollectionsStatement::database_id() const noexcept { return database_id_; }

} // namespace litedb::core::binder::bound
