#include "core/physical_planner/plan/query/query_plan.hpp"

#include <utility>

namespace litedb::core::physical_planner::plan
{

QueryPlan::QueryPlan(std::unique_ptr<op::PhysicalOperator> root_operator)
    : PhysicalPlan(PhysicalPlanKind::Query)
    , root_operator_(std::move(root_operator))
{
}

const op::PhysicalOperator & QueryPlan::root_operator() const noexcept
{
    return *root_operator_;
}

} // namespace litedb::core::physical_planner::plan
