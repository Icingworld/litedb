#include "core/physical_planner/plan/query/query_plan.hpp"

#include <utility>
#include <cassert>

namespace litedb::core::physical_planner::plan
{

QueryPlan::QueryPlan(std::unique_ptr<op::PhysicalOperator> root_operator)
    : PhysicalPlan(PhysicalPlanKind::Query)
    , root_operator_(std::move(root_operator))
{
    assert(root_operator_ != nullptr);
}

const op::PhysicalOperator & QueryPlan::root_operator() const noexcept
{
    return *root_operator_;
}

} // namespace litedb::core::physical_planner::plan
