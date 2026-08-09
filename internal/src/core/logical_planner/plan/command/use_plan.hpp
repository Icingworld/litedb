#pragma once

#include "core/logical_planner/plan/logical_plan.hpp"
#include "core/common/ids.hpp"

namespace litedb::core::logical_planner::plan
{

// USE 语句逻辑计划
class UsePlan final : public LogicalPlan
{
public:
    explicit UsePlan(common::DatabaseId database_id) noexcept;

public:
    // 获取数据库 ID
    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

private:
    common::DatabaseId database_id_;
};

} // namespace litedb::core::logical_planner::plan
