#include "core/logical_planner/plan/command/describe_collection_plan.hpp"

namespace litedb::core::logical_planner::plan
{

DescribeCollectionPlan::DescribeCollectionPlan(common::CollectionId collection_id) noexcept
    : LogicalPlan(LogicalPlanKind::DescribeCollection)
    , collection_id_(collection_id)
{}

common::CollectionId DescribeCollectionPlan::collection_id() const noexcept
{
    return collection_id_;
}

} // namespace litedb::core::logical_planner::plan
