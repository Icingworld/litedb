#include "core/binder/bound/statement/bound_insert_statement.hpp"

#include <utility>

namespace litedb::core::binder::bound
{

BoundInsertStatement::BoundInsertStatement(
    common::DatabaseId database_id,
    common::CollectionId collection_id,
    std::string collection_name,
    std::vector<BoundColumn> columns,
    std::vector<std::unique_ptr<BoundExpression>> values,
    parser::ast::AstNodeLocation location
)
    : BoundStatement(BoundStatementKind::Insert, location),
      database_id_(database_id),
      collection_id_(collection_id),
      collection_name_(std::move(collection_name)),
      columns_(std::move(columns)),
      values_(std::move(values))
{
}

common::DatabaseId BoundInsertStatement::database_id() const noexcept { return database_id_; }
common::CollectionId BoundInsertStatement::collection_id() const noexcept { return collection_id_; }
const std::string & BoundInsertStatement::collection_name() const noexcept { return collection_name_; }
const std::vector<BoundColumn> & BoundInsertStatement::columns() const noexcept { return columns_; }
const std::vector<std::unique_ptr<BoundExpression>> & BoundInsertStatement::values() const noexcept { return values_; }

} // namespace litedb::core::binder::bound
