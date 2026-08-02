#pragma once

#include <memory>

#include "core/logical_planner/node/logical_plan_node.hpp"
#include "core/logical_planner/plan/logical_statement_plan.hpp"
#include "core/physical_planner/node/physical_plan_node.hpp"
#include "core/physical_planner/statement/physical_statement_plan.hpp"

namespace litedb::core::physical_plan
{

class PhysicalPlanner
{
public:
    [[nodiscard]]
    std::unique_ptr<PhysicalStatementPlan> plan(const logical_planner::plan::LogicalStatementPlan & statement) const;

    [[nodiscard]]
    std::unique_ptr<PhysicalPlanNode> plan(const planner::logical::LogicalPlanNode & logical_root) const;
};

} // namespace litedb::core::physical_plan
