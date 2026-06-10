#include "core/binder/bound/statement/bound_describe_collection_statement.hpp"

#include <utility>

namespace litedb::core::binder::bound
{

BoundDescribeCollectionStatement::BoundDescribeCollectionStatement(
    common::DatabaseId database_id,
    common::CollectionId collection_id,
    std::string collection_name,
    parser::ast::AstNodeLocation location
)
    : BoundStatement(BoundStatementKind::DescribeCollection, location),
      database_id_(database_id),
      collection_id_(collection_id),
      collection_name_(std::move(collection_name))
{
}

common::DatabaseId BoundDescribeCollectionStatement::database_id() const noexcept { return database_id_; }
common::CollectionId BoundDescribeCollectionStatement::collection_id() const noexcept { return collection_id_; }
const std::string & BoundDescribeCollectionStatement::collection_name() const noexcept { return collection_name_; }

} // namespace litedb::core::binder::bound
