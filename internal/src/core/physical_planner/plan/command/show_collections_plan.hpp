#pragma once

#include "core/common/ids.hpp"
#include "core/physical_planner/plan/physical_plan.hpp"

namespace litedb::core::physical_planner::plan
{

/**
 * @brief SHOW COLLECTIONS 语句计划
 */
class ShowCollectionsPlan final : public PhysicalPlan
{
public:
    explicit ShowCollectionsPlan(common::DatabaseId database_id) noexcept;

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

} // namespace litedb::core::physical_planner::plan