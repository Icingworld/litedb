#pragma once

#include <memory>

namespace litedb::core::binder::bound
{

class BoundSelectStatement;

} // namespace litedb::core::binder::bound

namespace litedb::core::logical_planner::plan
{

class LogicalPlan;

} // namespace litedb::core::logical_planner::plan

namespace litedb::core::logical_planner
{

// SELECT 语句逻辑计划工作器
class LogicalPlannerSelectWorker
{
public:
    LogicalPlannerSelectWorker() = default;

public:
    // 规划 SELECT 语句
    [[nodiscard]]
    std::unique_ptr<plan::LogicalPlan>
    plan_select(
        binder::bound::BoundSelectStatement & statement
    );
};

} // namespace litedb::core::logical_planner
