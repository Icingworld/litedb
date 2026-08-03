#pragma once

#include <expected>
#include <memory>

#include "core/logical_planner/logical_planner_error.hpp"

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

/**
 * @brief DESCRIBE 语句逻辑计划工作器
 */
class LogicalPlannerDescribeWorker
{
public:
    LogicalPlannerDescribeWorker() = default;

public:
    /**
     * @brief 规划 DESCRIBE COLLECTION 语句
     * @param statement DESCRIBE COLLECTION 语句
     * @return 逻辑计划
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<plan::LogicalPlan>, LogicalPlannerError>
    plan_describe_collection(
        const binder::bound::BoundDescribeCollectionStatement & statement
    );
};

} // namespace litedb::core::logical_planner
