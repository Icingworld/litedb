#pragma once

#include <memory>

namespace litedb::core::binder::bound
{

class BoundInsertStatement;

} // namespace litedb::core::binder::bound

namespace litedb::core::logical_planner::plan
{

class LogicalPlan;

} // namespace litedb::core::logical_planner::plan

namespace litedb::core::logical_planner
{

// INSERT 语句逻辑计划工作器
class LogicalPlannerInsertWorker
{
public:
    LogicalPlannerInsertWorker() = default;

public:
    // 规划 INSERT 语句
    [[nodiscard]]
    std::unique_ptr<plan::LogicalPlan> plan_insert(binder::bound::BoundInsertStatement & statement);
};

} // namespace litedb::core::logical_planner
