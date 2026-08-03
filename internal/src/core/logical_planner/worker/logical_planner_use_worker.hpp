#pragma once

#include <expected>
#include <memory>

#include "core/logical_planner/logical_planner_error.hpp"

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

/**
 * @brief USE 语句逻辑计划工作器
 */
class LogicalPlannerUseWorker
{
public:
    LogicalPlannerUseWorker() = default;

public:
    /**
     * @brief 规划 USE 语句
     * @param statement USE 语句
     * @return 逻辑计划
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<plan::LogicalPlan>, LogicalPlannerError>
    plan_use(
        const binder::bound::BoundUseStatement & statement
    );
};

} // namespace litedb::core::logical_planner
