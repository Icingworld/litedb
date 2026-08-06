#pragma once

#include "core/physical_planner/plan/physical_plan.hpp"

namespace litedb::core::physical_planner::plan
{

/**
 * @brief SHOW DATABASES 语句计划
 */
class ShowDatabasesPlan final : public PhysicalPlan
{
public:
    ShowDatabasesPlan() noexcept;
};

} // namespace litedb::core::physical_planner::plan