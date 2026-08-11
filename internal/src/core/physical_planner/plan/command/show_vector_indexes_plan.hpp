#pragma once

#include "core/common/ids.hpp"
#include "core/physical_planner/plan/physical_plan.hpp"

namespace litedb::core::physical_planner::plan
{

// SHOW VINDEXES 语句物理计划
class ShowVectorIndexesPlan final : public PhysicalPlan
{
public:
    explicit ShowVectorIndexesPlan(common::CollectionId collection_id) noexcept;

public:
    // 获取集合 ID
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

private:
    common::CollectionId collection_id_;
};

} // namespace litedb::core::physical_planner::plan