#pragma once

#include <memory>

namespace litedb::core::binder::bound
{

class BoundUpdateStatement;

} // namespace litedb::core::binder::bound

namespace litedb::core::logical_planner::plan
{

class LogicalPlan;

} // namespace litedb::core::logical_planner::plan

namespace litedb::core::logical_planner
{

// UPDATE 语句逻辑计划工作器
class LogicalPlannerUpdateWorker
{
public:
    LogicalPlannerUpdateWorker() = default;

public:
    // 规划 UPDATE 语句
    [[nodiscard]]
    std::unique_ptr<plan::LogicalPlan>
    plan_update(
        binder::bound::BoundUpdateStatement & statement
    );
};

} // namespace litedb::core::logical_planner
