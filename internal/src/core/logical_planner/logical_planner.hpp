#pragma once

#include <memory>

namespace litedb::core::binder::bound
{

class BoundStatement;

} // namespace litedb::core::binder::bound

namespace litedb::core::logical_planner::plan
{

class LogicalPlan;

} // namespace litedb::core::logical_planner::plan

namespace litedb::core::logical_planner
{

/**
 * @brief 逻辑计划器
 */
class LogicalPlanner
{
public:
    /**
     * @brief 生成逻辑计划
     * @param statement 绑定语句
     * @return 逻辑计划
     * @pre statement != nullptr
     * @warning 该成员函数会消费 statement 的所有权
     */
    [[nodiscard]]
    std::unique_ptr<plan::LogicalPlan>
    plan(
        std::unique_ptr<binder::bound::BoundStatement> statement
    ) const;
};

} // namespace litedb::core::logical_planner
