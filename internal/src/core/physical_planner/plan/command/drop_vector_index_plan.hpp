#pragma once

#include <optional>

#include "core/common/ids.hpp"
#include "core/physical_planner/plan/physical_plan.hpp"

namespace litedb::core::physical_planner::plan
{

// DROP VINDEX 语句物理计划
class DropVectorIndexPlan final : public PhysicalPlan
{
public:
    explicit DropVectorIndexPlan(
        std::optional<common::VIndexId> index_id
    ) noexcept;

public:
    // 获取向量索引 ID
    [[nodiscard]]
    std::optional<common::VIndexId> index_id() const noexcept;

private:
    std::optional<common::VIndexId> index_id_;
};

} // namespace litedb::core::physical_planner::plan