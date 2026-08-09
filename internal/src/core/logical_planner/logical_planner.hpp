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

// 逻辑计划器
class LogicalPlanner
{
public:
    // 生成逻辑计划
    // 消费 statement 本身，并将其中的拥有型成员转移到逻辑计划
    [[nodiscard]]
    std::unique_ptr<plan::LogicalPlan> plan(
        std::unique_ptr<binder::bound::BoundStatement> statement
    ) const;
};

} // namespace litedb::core::logical_planner
