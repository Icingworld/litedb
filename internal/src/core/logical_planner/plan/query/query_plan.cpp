#include "core/logical_planner/plan/query/query_plan.hpp"

#include <utility>

namespace litedb::core::logical_planner::plan
{

QueryPlan::QueryPlan(std::unique_ptr<op::LogicalPlanOperator> root_operator)
    : LogicalPlan(LogicalPlanKind::Query)
    , root_operator_(std::move(root_operator))
{
}

const op::LogicalPlanOperator & QueryPlan::root_operator() const noexcept
{
    return *root_operator_;
}

std::unique_ptr<op::LogicalPlanOperator>
QueryPlan::take_root_operator() noexcept
{
    return std::move(root_operator_);
}

} // namespace litedb::core::logical_planner::plan
