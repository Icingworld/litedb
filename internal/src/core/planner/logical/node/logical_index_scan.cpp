#include "core/planner/logical/node/logical_index_scan.hpp"

#include <utility>

namespace litedb::core::planner::logical
{

IndexLookup::IndexLookup(IndexLookupKind kind, index::ScalarIndexKey key, index::IndexRange range)
    : kind(kind)
    , key(std::move(key))
    , range(std::move(range))
{
}

IndexLookup IndexLookup::equal(index::ScalarIndexKey key)
{
    auto range_key = key;
    return IndexLookup(IndexLookupKind::Equal, std::move(key), index::IndexRange::closed(range_key, range_key));
}

IndexLookup IndexLookup::range_scan(index::IndexRange range)
{
    return IndexLookup(IndexLookupKind::Range, index::ScalarIndexKey {}, std::move(range));
}

LogicalIndexScan::LogicalIndexScan(
    common::DatabaseId database_id,
    common::CollectionId collection_id,
    std::string collection_name,
    common::IndexId index_id,
    index::IndexKind index_kind,
    common::ColumnId column_id,
    IndexLookup lookup,
    parser::ast::AstNodeLocation location
)
    : LogicalPlanNode(LogicalPlanNodeKind::IndexScan, location)
    , database_id_(database_id)
    , collection_id_(collection_id)
    , collection_name_(std::move(collection_name))
    , index_id_(index_id)
    , index_kind_(index_kind)
    , column_id_(column_id)
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

index::IndexKind LogicalIndexScan::index_kind() const noexcept
{
    return index_kind_;
}

common::ColumnId LogicalIndexScan::column_id() const noexcept
{
    return column_id_;
}

const IndexLookup & LogicalIndexScan::lookup() const noexcept
{
    return lookup_;
}

} // namespace litedb::core::planner::logical
