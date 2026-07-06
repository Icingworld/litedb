#include "core/planner/logical/node/logical_scan.hpp"

#include <memory>
#include <utility>

namespace litedb::core::planner::logical
{

LogicalScan::LogicalScan(
    common::DatabaseId database_id,
    common::CollectionId collection_id,
    std::string collection_name,
    parser::ast::AstNodeLocation location
)
    : LogicalPlanNode(LogicalPlanNodeKind::Scan, location)
    , database_id_(database_id)
    , collection_id_(collection_id)
    , collection_name_(std::move(collection_name))
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

void LogicalScan::accept(LogicalPlanNodeVisitor & visitor) const
{
    visitor.visit(*this);
}

std::unique_ptr<LogicalPlanNode> LogicalScan::clone() const
{
    return std::make_unique<LogicalScan>(database_id_, collection_id_, collection_name_, location());
}

} // namespace litedb::core::planner::logical
