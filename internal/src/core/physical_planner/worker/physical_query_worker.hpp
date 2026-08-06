#pragma once

#include <memory>

#include "core/physical_planner/physical_planner_context.hpp"

namespace litedb::core::logical_planner::plan
{

class QueryPlan;

} // namespace litedb::core::logical_planner::plan

namespace litedb::core::physical_planner::plan
{

class PhysicalPlan;

} // namespace litedb::core::physical_planner::plan

namespace litedb::core::physical_planner
{

/**
 * @brief 查询类逻辑计划 lowering worker
 */
class PhysicalQueryWorker final
{
public:
    explicit PhysicalQueryWorker(const PhysicalPlannerContext & context) noexcept
        : context_(context)
    {
    }

public:
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan> plan_query(
        logical_planner::plan::QueryPlan & logical_plan
    );

private:
    const PhysicalPlannerContext & context_;
};

} // namespace litedb::core::physical_planner
