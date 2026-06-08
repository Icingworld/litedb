#include "core/planner/logical/logical_plan_node.hpp"

#include <utility>

namespace litedb::core::planner::logical
{

LogicalPlanNode::LogicalPlanNode(LogicalPlanNodeKind kind, parser::ast::AstNodeLocation location) noexcept
    : kind_(kind),
      location_(location)
{
}

LogicalPlanNodeKind LogicalPlanNode::kind() const noexcept { return kind_; }
parser::ast::AstNodeLocation LogicalPlanNode::location() const noexcept { return location_; }

LogicalUse::LogicalUse(common::DatabaseId database_id, std::string database_name, parser::ast::AstNodeLocation location)
    : LogicalPlanNode(LogicalPlanNodeKind::Use, location),
      database_id_(database_id),
      database_name_(std::move(database_name))
{
}

common::DatabaseId LogicalUse::database_id() const noexcept { return database_id_; }
const std::string & LogicalUse::database_name() const noexcept { return database_name_; }

LogicalCreateDatabase::LogicalCreateDatabase(
    std::string database_name,
    bool if_not_exists,
    parser::ast::AstNodeLocation location
)
    : LogicalPlanNode(LogicalPlanNodeKind::CreateDatabase, location),
      database_name_(std::move(database_name)),
      if_not_exists_(if_not_exists)
{
}

const std::string & LogicalCreateDatabase::database_name() const noexcept { return database_name_; }
bool LogicalCreateDatabase::if_not_exists() const noexcept { return if_not_exists_; }

LogicalCreateCollection::LogicalCreateCollection(
    common::DatabaseId database_id,
    std::string collection_name,
    bool if_not_exists,
    std::vector<catalog::ColumnDefinition> columns,
    parser::ast::AstNodeLocation location
)
    : LogicalPlanNode(LogicalPlanNodeKind::CreateCollection, location),
      database_id_(database_id),
      collection_name_(std::move(collection_name)),
      if_not_exists_(if_not_exists),
      columns_(std::move(columns))
{
}

common::DatabaseId LogicalCreateCollection::database_id() const noexcept { return database_id_; }
const std::string & LogicalCreateCollection::collection_name() const noexcept { return collection_name_; }
bool LogicalCreateCollection::if_not_exists() const noexcept { return if_not_exists_; }
const std::vector<catalog::ColumnDefinition> & LogicalCreateCollection::columns() const noexcept { return columns_; }

LogicalDropDatabase::LogicalDropDatabase(
    std::optional<common::DatabaseId> database_id,
    std::string database_name,
    bool if_exists,
    parser::ast::AstNodeLocation location
)
    : LogicalPlanNode(LogicalPlanNodeKind::DropDatabase, location),
      database_id_(database_id),
      database_name_(std::move(database_name)),
      if_exists_(if_exists)
{
}

std::optional<common::DatabaseId> LogicalDropDatabase::database_id() const noexcept { return database_id_; }
const std::string & LogicalDropDatabase::database_name() const noexcept { return database_name_; }
bool LogicalDropDatabase::if_exists() const noexcept { return if_exists_; }

LogicalDropCollection::LogicalDropCollection(
    common::DatabaseId database_id,
    std::optional<common::CollectionId> collection_id,
    std::string collection_name,
    bool if_exists,
    parser::ast::AstNodeLocation location
)
    : LogicalPlanNode(LogicalPlanNodeKind::DropCollection, location),
      database_id_(database_id),
      collection_id_(collection_id),
      collection_name_(std::move(collection_name)),
      if_exists_(if_exists)
{
}

common::DatabaseId LogicalDropCollection::database_id() const noexcept { return database_id_; }
std::optional<common::CollectionId> LogicalDropCollection::collection_id() const noexcept { return collection_id_; }
const std::string & LogicalDropCollection::collection_name() const noexcept { return collection_name_; }
bool LogicalDropCollection::if_exists() const noexcept { return if_exists_; }

LogicalShowDatabases::LogicalShowDatabases(parser::ast::AstNodeLocation location)
    : LogicalPlanNode(LogicalPlanNodeKind::ShowDatabases, location)
{
}

LogicalShowCollections::LogicalShowCollections(common::DatabaseId database_id, parser::ast::AstNodeLocation location)
    : LogicalPlanNode(LogicalPlanNodeKind::ShowCollections, location),
      database_id_(database_id)
{
}

common::DatabaseId LogicalShowCollections::database_id() const noexcept { return database_id_; }

LogicalDescribeCollection::LogicalDescribeCollection(
    common::DatabaseId database_id,
    common::CollectionId collection_id,
    std::string collection_name,
    parser::ast::AstNodeLocation location
)
    : LogicalPlanNode(LogicalPlanNodeKind::DescribeCollection, location),
      database_id_(database_id),
      collection_id_(collection_id),
      collection_name_(std::move(collection_name))
{
}

common::DatabaseId LogicalDescribeCollection::database_id() const noexcept { return database_id_; }
common::CollectionId LogicalDescribeCollection::collection_id() const noexcept { return collection_id_; }
const std::string & LogicalDescribeCollection::collection_name() const noexcept { return collection_name_; }

LogicalScan::LogicalScan(
    common::DatabaseId database_id,
    common::CollectionId collection_id,
    std::string collection_name,
    parser::ast::AstNodeLocation location
)
    : LogicalPlanNode(LogicalPlanNodeKind::Scan, location),
      database_id_(database_id),
      collection_id_(collection_id),
      collection_name_(std::move(collection_name))
{
}

common::DatabaseId LogicalScan::database_id() const noexcept { return database_id_; }
common::CollectionId LogicalScan::collection_id() const noexcept { return collection_id_; }
const std::string & LogicalScan::collection_name() const noexcept { return collection_name_; }

LogicalUnaryNode::LogicalUnaryNode(
    LogicalPlanNodeKind kind,
    std::unique_ptr<LogicalPlanNode> child,
    parser::ast::AstNodeLocation location
) noexcept
    : LogicalPlanNode(kind, location),
      child_(std::move(child))
{
}

const LogicalPlanNode & LogicalUnaryNode::child() const noexcept { return *child_; }

LogicalFilter::LogicalFilter(
    std::unique_ptr<LogicalPlanNode> child,
    std::unique_ptr<binder::bound::BoundExpression> predicate,
    parser::ast::AstNodeLocation location
)
    : LogicalUnaryNode(LogicalPlanNodeKind::Filter, std::move(child), location),
      predicate_(std::move(predicate))
{
}

const binder::bound::BoundExpression & LogicalFilter::predicate() const noexcept { return *predicate_; }

LogicalProjection::LogicalProjection(
    std::unique_ptr<LogicalPlanNode> child,
    std::vector<std::unique_ptr<binder::bound::BoundExpression>> projections,
    parser::ast::AstNodeLocation location
)
    : LogicalUnaryNode(LogicalPlanNodeKind::Projection, std::move(child), location),
      projections_(std::move(projections))
{
}

const std::vector<std::unique_ptr<binder::bound::BoundExpression>> & LogicalProjection::projections() const noexcept
{
    return projections_;
}

LogicalOrderBy::LogicalOrderBy(
    std::unique_ptr<LogicalPlanNode> child,
    std::vector<binder::bound::BoundOrderByItem> order_by,
    parser::ast::AstNodeLocation location
)
    : LogicalUnaryNode(LogicalPlanNodeKind::OrderBy, std::move(child), location),
      order_by_(std::move(order_by))
{
}

const std::vector<binder::bound::BoundOrderByItem> & LogicalOrderBy::order_by() const noexcept { return order_by_; }

LogicalLimit::LogicalLimit(
    std::unique_ptr<LogicalPlanNode> child,
    std::optional<std::size_t> limit,
    std::optional<std::size_t> offset,
    parser::ast::AstNodeLocation location
)
    : LogicalUnaryNode(LogicalPlanNodeKind::Limit, std::move(child), location),
      limit_(limit),
      offset_(offset)
{
}

std::optional<std::size_t> LogicalLimit::limit() const noexcept { return limit_; }
std::optional<std::size_t> LogicalLimit::offset() const noexcept { return offset_; }

LogicalInsert::LogicalInsert(
    common::DatabaseId database_id,
    common::CollectionId collection_id,
    std::string collection_name,
    std::vector<binder::bound::BoundColumn> columns,
    std::vector<std::unique_ptr<binder::bound::BoundExpression>> values,
    parser::ast::AstNodeLocation location
)
    : LogicalPlanNode(LogicalPlanNodeKind::Insert, location),
      database_id_(database_id),
      collection_id_(collection_id),
      collection_name_(std::move(collection_name)),
      columns_(std::move(columns)),
      values_(std::move(values))
{
}

common::DatabaseId LogicalInsert::database_id() const noexcept { return database_id_; }
common::CollectionId LogicalInsert::collection_id() const noexcept { return collection_id_; }
const std::string & LogicalInsert::collection_name() const noexcept { return collection_name_; }
const std::vector<binder::bound::BoundColumn> & LogicalInsert::columns() const noexcept { return columns_; }
const std::vector<std::unique_ptr<binder::bound::BoundExpression>> & LogicalInsert::values() const noexcept
{
    return values_;
}

LogicalUpdate::LogicalUpdate(
    std::unique_ptr<LogicalPlanNode> child,
    common::DatabaseId database_id,
    common::CollectionId collection_id,
    std::string collection_name,
    std::vector<binder::bound::BoundAssignment> assignments,
    parser::ast::AstNodeLocation location
)
    : LogicalUnaryNode(LogicalPlanNodeKind::Update, std::move(child), location),
      database_id_(database_id),
      collection_id_(collection_id),
      collection_name_(std::move(collection_name)),
      assignments_(std::move(assignments))
{
}

common::DatabaseId LogicalUpdate::database_id() const noexcept { return database_id_; }
common::CollectionId LogicalUpdate::collection_id() const noexcept { return collection_id_; }
const std::string & LogicalUpdate::collection_name() const noexcept { return collection_name_; }
const std::vector<binder::bound::BoundAssignment> & LogicalUpdate::assignments() const noexcept { return assignments_; }

LogicalDelete::LogicalDelete(
    std::unique_ptr<LogicalPlanNode> child,
    common::DatabaseId database_id,
    common::CollectionId collection_id,
    std::string collection_name,
    parser::ast::AstNodeLocation location
)
    : LogicalUnaryNode(LogicalPlanNodeKind::Delete, std::move(child), location),
      database_id_(database_id),
      collection_id_(collection_id),
      collection_name_(std::move(collection_name))
{
}

common::DatabaseId LogicalDelete::database_id() const noexcept { return database_id_; }
common::CollectionId LogicalDelete::collection_id() const noexcept { return collection_id_; }
const std::string & LogicalDelete::collection_name() const noexcept { return collection_name_; }

} // namespace litedb::core::planner::logical
