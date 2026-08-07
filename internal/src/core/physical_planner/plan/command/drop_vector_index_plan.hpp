#pragma once

#include <optional>

#include "core/common/ids.hpp"
#include "core/physical_planner/plan/physical_plan.hpp"

namespace litedb::core::physical_planner::plan
{

/**
 * @brief DROP VINDEX 语句计划
 */
class DropVectorIndexPlan final : public PhysicalPlan
{
public:
    explicit DropVectorIndexPlan(
        std::optional<common::VIndexId> index_id
    ) noexcept;

public:
    /**
     * @brief 获取向量索引 ID
     * @return 向量索引 ID
     */
    [[nodiscard]]
    std::optional<common::VIndexId> index_id() const noexcept;

private:
    std::optional<common::VIndexId> index_id_;  // 向量索引 ID
};

} // namespace litedb::core::physical_planner::plan