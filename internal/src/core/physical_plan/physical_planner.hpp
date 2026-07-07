#pragma once

#include <memory>

#include "core/logical_plan/node/logical_plan_node.hpp"
#include "core/physical_plan/node/physical_plan_node.hpp"

namespace litedb::core::physical_plan
{

class PhysicalPlanner
{
public:
    [[nodiscard]]
    std::unique_ptr<PhysicalPlanNode> plan(const planner::logical::LogicalPlanNode & logical_root) const;
};

} // namespace litedb::core::physical_plan
