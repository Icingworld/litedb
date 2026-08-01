#include "core/physical_planner/node/physical_index_scan.hpp"

#include <memory>
#include <utility>

namespace litedb::core::physical_plan
{

PhysicalIndexScan::PhysicalIndexScan(
    common::DatabaseId database_id,
    common::CollectionId collection_id,
    std::string collection_name,
    common::IndexId index_id,
    std::string index_name,
    meta::entry::IndexKind index_kind,
    common::ColumnId column_id,
    std::string column_name,
    PhysicalIndexLookup lookup,
    parser::ast::AstNodeLocation location
)
    : PhysicalPlanNode(PhysicalPlanNodeKind::IndexScan, location)
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

common::DatabaseId PhysicalIndexScan::database_id() const noexcept
{
    return database_id_;
}

common::CollectionId PhysicalIndexScan::collection_id() const noexcept
{
    return collection_id_;
}

const std::string & PhysicalIndexScan::collection_name() const noexcept
{
    return collection_name_;
}

common::IndexId PhysicalIndexScan::index_id() const noexcept
{
    return index_id_;
}

const std::string & PhysicalIndexScan::index_name() const noexcept
{
    return index_name_;
}

meta::entry::IndexKind PhysicalIndexScan::index_kind() const noexcept
{
    return index_kind_;
}

common::ColumnId PhysicalIndexScan::column_id() const noexcept
{
    return column_id_;
}

const std::string & PhysicalIndexScan::column_name() const noexcept
{
    return column_name_;
}

const PhysicalIndexLookup & PhysicalIndexScan::lookup() const noexcept
{
    return lookup_;
}

std::unique_ptr<PhysicalPlanNode> PhysicalIndexScan::clone() const
{
    return std::make_unique<PhysicalIndexScan>(
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

} // namespace litedb::core::physical_plan
