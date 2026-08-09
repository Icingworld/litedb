#pragma once

#include "core/common/ids.hpp"
#include "core/logical_planner/plan/logical_plan.hpp"

namespace litedb::core::logical_planner::plan
{

// SHOW COLLECTIONS 语句逻辑计划
class ShowCollectionsPlan final : public LogicalPlan
{
public:
    explicit ShowCollectionsPlan(common::DatabaseId database_id) noexcept;

public:
    // 获取数据库 ID
    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

private:
    common::DatabaseId database_id_;
};

} // namespace litedb::core::logical_planner::plan
