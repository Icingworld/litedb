#pragma once

#include <optional>

#include "core/common/ids.hpp"
#include "core/logical_planner/plan/logical_plan.hpp"

namespace litedb::core::logical_planner::plan
{

// DROP COLLECTION 语句逻辑计划
class DropCollectionPlan final : public LogicalPlan
{
public:
    explicit DropCollectionPlan(std::optional<common::CollectionId> collection_id) noexcept;

public:
    // 获取集合 ID
    [[nodiscard]]
    std::optional<common::CollectionId> collection_id() const noexcept;

private:
    std::optional<common::CollectionId> collection_id_;
};

} // namespace litedb::core::logical_planner::plan
