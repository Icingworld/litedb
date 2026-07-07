#include "core/planner/logical/node/logical_index_scan.hpp"

#include <memory>
#include <utility>

namespace litedb::core::planner::logical
{

LogicalIndexScan::LogicalIndexScan(
    common::DatabaseId database_id,
    common::CollectionId collection_id,
    std::string collection_name,
    common::IndexId index_id,
    std::string index_name,
    catalog::CatalogIndexKind index_kind,
    common::ColumnId column_id,
    std::string column_name,
    LogicalIndexLookup lookup,
    parser::ast::AstNodeLocation location
)
    : LogicalPlanNode(LogicalPlanNodeKind::IndexScan, location)
    , database_id_(database_id)
    , collection_id_(collection_id)
    , collection_name_(std::move(collection_name))
    , index_id_(index_id)
    , index_name_(std::move(index_name))
    , index_kind_(index_kind)
    , column_id_(column_id)
    , column_name_(std::move(column_name))
    , lookup_(std::move(lookup))
{
}

common::DatabaseId LogicalIndexScan::database_id() const noexcept
{
    return database_id_;
}

common::CollectionId LogicalIndexScan::collection_id() const noexcept
{
    return collection_id_;
}

const std::string & LogicalIndexScan::collection_name() const noexcept
{
    return collection_name_;
}

common::IndexId LogicalIndexScan::index_id() const noexcept
{
    return index_id_;
}

const std::string & LogicalIndexScan::index_name() const noexcept
{
    return index_name_;
}

catalog::CatalogIndexKind LogicalIndexScan::index_kind() const noexcept
{
    return index_kind_;
}

common::ColumnId LogicalIndexScan::column_id() const noexcept
{
    return column_id_;
}

const std::string & LogicalIndexScan::column_name() const noexcept
{
    return column_name_;
}

const LogicalIndexLookup & LogicalIndexScan::lookup() const noexcept
{
    return lookup_;
}

void LogicalIndexScan::accept(LogicalPlanNodeVisitor & visitor) const
{
    visitor.visit(*this);
}

std::unique_ptr<LogicalPlanNode> LogicalIndexScan::clone() const
{
    return std::make_unique<LogicalIndexScan>(
        database_id_,
        collection_id_,
        collection_name_,
        index_id_,
        index_name_,
        index_kind_,
        column_id_,
        column_name_,
        lookup_,
        location()
    );
}

} // namespace litedb::core::planner::logical
