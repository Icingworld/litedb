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

// 查询类物理计划工作器
class PhysicalQueryWorker final
{
public:
    explicit PhysicalQueryWorker(const PhysicalPlannerContext & context) noexcept;

public:
    // 计划 QUERY 语句
    [[nodiscard]] std::unique_ptr<plan::PhysicalPlan> plan_query(
        logical_planner::plan::QueryPlan & logical_plan
    );

private:
    const PhysicalPlannerContext & context_;
};

} // namespace litedb::core::physical_planner
