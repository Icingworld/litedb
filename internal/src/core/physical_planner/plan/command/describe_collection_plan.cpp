#include "core/physical_planner/plan/command/describe_collection_plan.hpp"

namespace litedb::core::physical_planner::plan
{

DescribeCollectionPlan::DescribeCollectionPlan(common::CollectionId collection_id) noexcept
    : PhysicalPlan(PhysicalPlanKind::DescribeCollection)
    , collection_id_(collection_id)
{}

common::CollectionId DescribeCollectionPlan::collection_id() const noexcept
{
    return collection_id_;
}

} // namespace litedb::core::physical_planner::plan