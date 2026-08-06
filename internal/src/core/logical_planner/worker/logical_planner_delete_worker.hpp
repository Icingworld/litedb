#pragma once

#include <memory>

namespace litedb::core::binder::bound
{

class BoundDeleteStatement;

} // namespace litedb::core::binder::bound

namespace litedb::core::logical_planner::plan
{

class LogicalPlan;

} // namespace litedb::core::logical_planner::plan

namespace litedb::core::logical_planner
{

/**
 * @brief DELETE 语句逻辑计划工作器
 */
class LogicalPlannerDeleteWorker
{
public:
    LogicalPlannerDeleteWorker() = default;

public:
    /**
     * @brief 规划 DELETE 语句
     * @param statement DELETE 语句
     * @return 逻辑计划
     */
    [[nodiscard]]
    std::unique_ptr<plan::LogicalPlan>
    plan_delete(
        binder::bound::BoundDeleteStatement & statement
    );
};

} // namespace litedb::core::logical_planner
