#pragma once

#include <memory>

#include "core/logical_planner/plan/logical_plan.hpp"
#include "core/meta/meta_engine.hpp"
#include "core/physical_planner/plan/physical_plan.hpp"

namespace litedb::core::physical_planner
{

/**
 * @brief 物理计划 facade
 * @details 只负责保存目录视图并把逻辑计划交给 PhysicalPlannerWorker。
 */
class PhysicalPlanner final
{
public:
    explicit PhysicalPlanner(meta::CatalogView catalog) noexcept;

public:
    /**
     * @brief 生成物理计划
     * @param logical_plan 逻辑计划
     * @return 物理计划
     * @pre logical_plan != nullptr
     * @warning 该成员函数会消耗 logical_plan 的所有权
     */
    [[nodiscard]]
    std::unique_ptr<plan::PhysicalPlan> plan(
        std::unique_ptr<logical_planner::plan::LogicalPlan> logical_plan
    );

private:
    meta::CatalogView catalog_;             // 目录视图
};

} // namespace litedb::core::physical_planner
