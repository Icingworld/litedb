#pragma once

#include <expected>
#include <memory>

#include "core/logical_planner/logical_planner_error.hpp"

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

/**
 * @brief SELECT 语句逻辑计划工作器
 */
class LogicalPlannerSelectWorker
{
public:
    LogicalPlannerSelectWorker() = default;

public:
    /**
     * @brief 规划 SELECT 语句
     * @param statement SELECT 语句
     * @return 逻辑计划
     * @warning 该成员函数将会移动消费 statement 的成员变量
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<plan::LogicalPlan>, LogicalPlannerError>
    plan_select(
        binder::bound::BoundSelectStatement & statement
    );
};

} // namespace litedb::core::logical_planner
