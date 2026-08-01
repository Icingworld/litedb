#include "core/logical_planner/node/logical_scan.hpp"

#include <memory>
#include <utility>

namespace litedb::core::planner::logical
{

LogicalScan::LogicalScan(
    common::DatabaseId database_id,
    common::CollectionId collection_id,
    std::string collection_name,
    std::optional<LogicalScanIndexHint> index_hint,
    parser::ast::AstNodeLocation location
)
    : LogicalPlanNode(LogicalPlanNodeKind::Scan, location)
    , database_id_(database_id)
    , collection_id_(collection_id)
    , collection_name_(std::move(collection_name))
    , index_hint_(std::move(index_hint))
{
}

LogicalScan::LogicalScan(
    common::DatabaseId database_id,
    common::CollectionId collection_id,
    std::string collection_name,
    parser::ast::AstNodeLocation location
)
    : LogicalScan(database_id, collection_id, std::move(collection_name), std::nullopt, location)
{
}

common::DatabaseId LogicalScan::database_id() const noexcept
{
    return database_id_;
}

common::CollectionId LogicalScan::collection_id() const noexcept
{
    return collection_id_;
}

const std::string & LogicalScan::collection_name() const noexcept
{
    return collection_name_;
}

const std::optional<LogicalScanIndexHint> & LogicalScan::index_hint() const noexcept
{
    return index_hint_;
}

void LogicalScan::accept(LogicalPlanNodeVisitor & visitor) const
{
    visitor.visit(*this);
}

std::unique_ptr<LogicalPlanNode> LogicalScan::clone() const
{
    return std::make_unique<LogicalScan>(database_id_, collection_id_, collection_name_, index_hint_, location());
}

} // namespace litedb::core::planner::logical
