#pragma once

#include <optional>

#include "core/logical_planner/plan/logical_plan.hpp"
#include "core/common/ids.hpp"

namespace litedb::core::logical_planner::plan
{

/**
 * @brief DROP INDEX 语句计划
 */
class DropIndexPlan final : public LogicalPlan
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

} // namespace litedb::core::logical_planner::plan
