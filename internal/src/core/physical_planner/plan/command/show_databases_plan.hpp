#pragma once

#include "core/physical_planner/plan/physical_plan.hpp"

namespace litedb::core::physical_planner::plan
{

// SHOW DATABASES 语句物理计划
class ShowDatabasesPlan final : public PhysicalPlan
{
public:
    ShowDatabasesPlan() noexcept;
};

} // namespace litedb::core::physical_planner::plan