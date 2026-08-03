#pragma once

#include <expected>
#include <memory>

#include "core/logical_planner/logical_planner_error.hpp"

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

/**
 * @brief INSERT 语句逻辑计划工作器
 */
class LogicalPlannerInsertWorker
{
public:
    LogicalPlannerInsertWorker() = default;

public:
    /**
     * @brief 规划 INSERT 语句
     * @param statement INSERT 语句
     * @return 逻辑计划
     * @warning 该成员函数将会移动消费 statement 的成员变量
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<plan::LogicalPlan>, LogicalPlannerError>
    plan_insert(
        binder::bound::BoundInsertStatement & statement
    );
};

} // namespace litedb::core::logical_planner
