#include "core/binder/bound/statement/bound_select_statement.hpp"

#include <utility>

namespace litedb::core::binder::bound
{

BoundSelectStatement::BoundSelectStatement(
    common::DatabaseId database_id,
    common::CollectionId collection_id,
    std::string collection_name,
    std::vector<std::unique_ptr<BoundExpression>> projections,
    std::unique_ptr<BoundExpression> where,
    std::vector<BoundOrderByItem> order_by,
    std::optional<std::size_t> limit,
    std::optional<std::size_t> offset,
    parser::ast::AstNodeLocation location
)
    : BoundStatement(BoundStatementKind::Select, location),
      database_id_(database_id),
      collection_id_(collection_id),
      collection_name_(std::move(collection_name)),
      projections_(std::move(projections)),
      where_(std::move(where)),
      order_by_(std::move(order_by)),
      limit_(limit),
      offset_(offset)
{
}

common::DatabaseId BoundSelectStatement::database_id() const noexcept { return database_id_; }
common::CollectionId BoundSelectStatement::collection_id() const noexcept { return collection_id_; }
const std::string & BoundSelectStatement::collection_name() const noexcept { return collection_name_; }
const std::vector<std::unique_ptr<BoundExpression>> & BoundSelectStatement::projections() const noexcept { return projections_; }
const BoundExpression * BoundSelectStatement::where() const noexcept { return where_.get(); }
const std::vector<BoundOrderByItem> & BoundSelectStatement::order_by() const noexcept { return order_by_; }
std::optional<std::size_t> BoundSelectStatement::limit() const noexcept { return limit_; }
std::optional<std::size_t> BoundSelectStatement::offset() const noexcept { return offset_; }

} // namespace litedb::core::binder::bound
