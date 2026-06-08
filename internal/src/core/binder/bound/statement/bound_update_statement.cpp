#include "core/binder/bound/statement/bound_update_statement.hpp"

#include <utility>

namespace litedb::core::binder::bound
{

BoundUpdateStatement::BoundUpdateStatement(
    common::DatabaseId database_id,
    common::CollectionId collection_id,
    std::string collection_name,
    std::vector<BoundAssignment> assignments,
    std::unique_ptr<BoundExpression> where,
    parser::ast::AstNodeLocation location
)
    : BoundStatement(BoundStatementKind::Update, location),
      database_id_(database_id),
      collection_id_(collection_id),
      collection_name_(std::move(collection_name)),
      assignments_(std::move(assignments)),
      where_(std::move(where))
{
}

common::DatabaseId BoundUpdateStatement::database_id() const noexcept { return database_id_; }
common::CollectionId BoundUpdateStatement::collection_id() const noexcept { return collection_id_; }
const std::string & BoundUpdateStatement::collection_name() const noexcept { return collection_name_; }
const std::vector<BoundAssignment> & BoundUpdateStatement::assignments() const noexcept { return assignments_; }
const BoundExpression * BoundUpdateStatement::where() const noexcept { return where_.get(); }

} // namespace litedb::core::binder::bound
