#include "core/physical_planner/operator/physical_seq_scan_operator.hpp"

namespace litedb::core::physical_planner::op
{

SeqScanOperator::SeqScanOperator(common::CollectionId collection_id) noexcept
    : PhysicalOperator(PhysicalOperatorKind::SeqScan)
    , collection_id_(collection_id)
{}

common::CollectionId SeqScanOperator::collection_id() const noexcept
{
    return collection_id_;
}

} // namespace litedb::core::physical_planner::op
