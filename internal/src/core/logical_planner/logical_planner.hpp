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
    // 该过程可能会获取 statement 成员的所有权
    [[nodiscard]]
    std::unique_ptr<plan::LogicalPlan> plan(
        std::unique_ptr<binder::bound::BoundStatement> statement
    ) const;
};

} // namespace litedb::core::logical_planner
