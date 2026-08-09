#pragma once

#include <optional>

#include "core/common/ids.hpp"
#include "core/logical_planner/plan/logical_plan.hpp"

namespace litedb::core::logical_planner::plan
{

// DROP DATABASE 语句逻辑计划
class DropDatabasePlan final : public LogicalPlan
{
public:
    explicit DropDatabasePlan(std::optional<common::DatabaseId> database_id) noexcept;

public:
    // 获取数据库 ID
    [[nodiscard]]
    std::optional<common::DatabaseId> database_id() const noexcept;

private:
    std::optional<common::DatabaseId> database_id_;
};

} // namespace litedb::core::logical_planner::plan
