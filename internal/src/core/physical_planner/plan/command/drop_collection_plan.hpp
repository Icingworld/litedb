#pragma once

#include <optional>

#include "core/common/ids.hpp"
#include "core/physical_planner/plan/physical_plan.hpp"

namespace litedb::core::physical_planner::plan
{

// DROP COLLECTION 语句物理计划
class DropCollectionPlan final : public PhysicalPlan
{
public:
    explicit DropCollectionPlan(
        std::optional<common::CollectionId> collection_id
    ) noexcept;

public:
    // 获取集合 ID
    [[nodiscard]]
    std::optional<common::CollectionId> collection_id() const noexcept;

private:
    std::optional<common::CollectionId> collection_id_;
};

} // namespace litedb::core::physical_planner::plan