#pragma once

#include <optional>

#include "core/common/ids.hpp"
#include "core/physical_planner/plan/physical_plan.hpp"

namespace litedb::core::physical_planner::plan
{

// DROP INDEX 语句物理计划
class DropIndexPlan final : public PhysicalPlan
{
public:
    explicit DropIndexPlan(
        std::optional<common::IndexId> index_id
    ) noexcept;

public:
    // 获取索引 ID
    [[nodiscard]]
    std::optional<common::IndexId> index_id() const noexcept;

private:
    std::optional<common::IndexId> index_id_;
};

} // namespace litedb::core::physical_planner::plan