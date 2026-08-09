#pragma once

#include <memory>

namespace litedb::core::binder::bound
{

class BoundDescribeCollectionStatement;

} // namespace litedb::core::binder::bound

namespace litedb::core::logical_planner::plan
{

class LogicalPlan;

} // namespace litedb::core::logical_planner::plan

namespace litedb::core::logical_planner
{

// DESCRIBE 语句逻辑计划工作器
class LogicalPlannerDescribeWorker
{
public:
    LogicalPlannerDescribeWorker() = default;

public:
    // 规划 DESCRIBE COLLECTION 语句
    [[nodiscard]]
    std::unique_ptr<plan::LogicalPlan>
    plan_describe_collection(
        const binder::bound::BoundDescribeCollectionStatement & statement
    );
};

} // namespace litedb::core::logical_planner
