#pragma once

#include <memory>

namespace litedb::core::binder::bound
{

class BoundUseStatement;

} // namespace litedb::core::binder::bound

namespace litedb::core::logical_planner::plan
{

class LogicalPlan;

} // namespace litedb::core::logical_planner::plan

namespace litedb::core::logical_planner
{

// USE 语句逻辑计划工作器
class LogicalPlannerUseWorker
{
public:
    LogicalPlannerUseWorker() = default;

public:
    // 规划 USE 语句
    [[nodiscard]]
    std::unique_ptr<plan::LogicalPlan> plan_use(const binder::bound::BoundUseStatement & statement);
};

} // namespace litedb::core::logical_planner
