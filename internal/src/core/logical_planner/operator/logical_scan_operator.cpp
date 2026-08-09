#include "core/logical_planner/operator/logical_scan_operator.hpp"

namespace litedb::core::logical_planner::op
{

LogicalScanOperator::LogicalScanOperator(common::CollectionId collection_id)
    : LogicalPlanOperator(LogicalPlanOperatorKind::Scan)
    , collection_id_(collection_id)
{}

common::CollectionId LogicalScanOperator::collection_id() const noexcept
{
    return collection_id_;
}

} // namespace litedb::core::logical_planner::op
