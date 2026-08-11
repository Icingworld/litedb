#include "core/physical_planner/operator/physical_index_scan_operator.hpp"

#include <utility>

namespace litedb::core::physical_planner::op
{

IndexScanOperator::IndexScanOperator(
    common::CollectionId collection_id,
    common::IndexId index_id,
    IndexLookup lookup
) noexcept
    : PhysicalOperator(PhysicalOperatorKind::IndexScan)
    , collection_id_(collection_id)
    , index_id_(index_id)
    , lookup_(std::move(lookup))
{}

common::CollectionId IndexScanOperator::collection_id() const noexcept
{
    return collection_id_;
}

common::IndexId IndexScanOperator::index_id() const noexcept
{
    return index_id_;
}

const IndexLookup & IndexScanOperator::lookup() const noexcept
{
    return lookup_;
}

} // namespace litedb::core::physical_planner::op
