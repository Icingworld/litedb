#pragma once

#include <optional>

#include "core/common/ids.hpp"
#include "core/physical_planner/plan/physical_plan.hpp"

namespace litedb::core::physical_planner::plan
{

/**
 * @brief DROP INDEX 语句计划
 */
class DropIndexPlan final : public PhysicalPlan
{
public:
    explicit DropIndexPlan(
        std::optional<common::IndexId> index_id
    ) noexcept;

public:
    /**
     * @brief 获取索引 ID
     * @return 索引 ID
     */
    [[nodiscard]]
    std::optional<common::IndexId> index_id() const noexcept;

private:
    std::optional<common::IndexId> index_id_;  // 索引 ID
};

} // namespace litedb::core::physical_planner::plan