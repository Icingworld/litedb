#pragma once

#include "core/logical_planner/plan/logical_plan.hpp"

namespace litedb::core::logical_planner::plan
{

// SHOW DATABASES 语句逻辑计划
class ShowDatabasesPlan final : public LogicalPlan
{
public:
    ShowDatabasesPlan() noexcept;
};

} // namespace litedb::core::logical_planner::plan
