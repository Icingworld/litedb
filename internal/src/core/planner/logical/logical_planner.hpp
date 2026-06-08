#pragma once

#include <expected>
#include <memory>

#include "core/binder/bound/statement/bound_statement.hpp"
#include "core/planner/logical/logical_plan_node.hpp"
#include "core/planner/logical/planner_error.hpp"

namespace litedb::core::planner::logical
{

class LogicalPlanner
{
public:
    [[nodiscard]]
    std::expected<std::unique_ptr<LogicalPlanNode>, PlannerError> plan(
        std::unique_ptr<binder::bound::BoundStatement> statement
    ) const;
};

} // namespace litedb::core::planner::logical
