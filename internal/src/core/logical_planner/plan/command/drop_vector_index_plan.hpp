#pragma once

#include <optional>

#include "core/common/ids.hpp"
#include "core/logical_planner/plan/logical_plan.hpp"

namespace litedb::core::logical_planner::plan
{

// DROP VINDEX 语句逻辑计划
class DropVectorIndexPlan final : public LogicalPlan
{
public:
    explicit DropVectorIndexPlan(std::optional<common::VIndexId> vector_index_id) noexcept;

public:
    // 获取向量索引 ID
    [[nodiscard]]
    std::optional<common::VIndexId> vector_index_id() const noexcept;

private:
    std::optional<common::VIndexId> vector_index_id_;
};

} // namespace litedb::core::logical_planner::plan
