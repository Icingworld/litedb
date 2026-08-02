#pragma once

#include <optional>

#include "core/logical_planner/plan/logical_plan.hpp"
#include "core/common/ids.hpp"


namespace litedb::core::logical_planner::plan
{

/**
 * @brief DROP VINDEX 语句计划
 */
class DropVectorIndexPlan final : public LogicalPlan
{
public:
    DropVectorIndexPlan(
        std::optional<common::VIndexId> vector_index_id
    ) noexcept;

public:
    /**
     * @brief 获取向量索引 ID
     * @return 向量索引 ID
     */
    [[nodiscard]]
    std::optional<common::VIndexId> vector_index_id() const noexcept;

private:
    std::optional<common::VIndexId> vector_index_id_;  ///< 向量索引 ID
};

} // namespace litedb::core::logical_planner::plan
