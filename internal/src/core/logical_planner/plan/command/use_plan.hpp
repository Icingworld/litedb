#pragma once

#include "core/logical_planner/plan/logical_plan.hpp"
#include "core/common/ids.hpp"

namespace litedb::core::logical_planner::plan
{

/**
 * @brief USE 语句计划
 */
class UsePlan final : public LogicalPlan
{
public:
    explicit UsePlan(common::DatabaseId database_id) noexcept;

public:
    /**
     * @brief 获取数据库 ID
     * @return 数据库 ID
     */
    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

private:
    common::DatabaseId database_id_;             // 数据库 ID
};

} // namespace litedb::core::logical_planner::plan
