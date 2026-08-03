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

/**
 * @brief UPDATE 语句逻辑计划工作器
 */
class LogicalPlannerUpdateWorker
{
public:
    LogicalPlannerUpdateWorker() = default;

public:
    /**
     * @brief 规划 UPDATE 语句
     * @param statement UPDATE 语句
     * @return 逻辑计划
     * @warning 该成员函数将会移动消费 statement 的成员变量
     */
    [[nodiscard]]
    std::unique_ptr<plan::LogicalPlan>
    plan_update(
        binder::bound::BoundUpdateStatement & statement
    );
};

} // namespace litedb::core::logical_planner
