#pragma once

#include <optional>

#include "core/common/ids.hpp"
#include "core/physical_planner/plan/physical_plan.hpp"

namespace litedb::core::physical_planner::plan
{

/**
 * @brief DROP DATABASE 语句计划
 */
class DropDatabasePlan final : public PhysicalPlan
{
public:
    explicit DropDatabasePlan(
        std::optional<common::DatabaseId> database_id
    ) noexcept;

public:
    /**
     * @brief 获取数据库 ID
     * @return 数据库 ID
     */
    [[nodiscard]]
    std::optional<common::DatabaseId> database_id() const noexcept;

private:
    std::optional<common::DatabaseId> database_id_;  // 数据库 ID
};

} // namespace litedb::core::physical_planner::plan