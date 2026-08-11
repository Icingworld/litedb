#pragma once

#include <memory>

#include "core/logical_planner/plan/logical_plan.hpp"
#include "core/meta/meta_engine.hpp"
#include "core/physical_planner/plan/physical_plan.hpp"

namespace litedb::core::physical_planner
{

// 物理计划器
class PhysicalPlanner final
{
public:
    explicit PhysicalPlanner(meta::CatalogView catalog) noexcept;

public:
    // 生成物理计划
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> plan(
        std::unique_ptr<logical_planner::plan::LogicalPlan> logical_plan
    );

private:
    meta::CatalogView catalog_;
};

} // namespace litedb::core::physical_planner
