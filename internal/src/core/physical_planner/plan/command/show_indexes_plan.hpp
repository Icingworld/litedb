#pragma once

#include "core/common/ids.hpp"
#include "core/physical_planner/plan/physical_plan.hpp"

namespace litedb::core::physical_planner::plan
{

// SHOW INDEXES 语句物理计划
class ShowIndexesPlan final : public PhysicalPlan
{
public:
    explicit ShowIndexesPlan(common::CollectionId collection_id) noexcept;

public:
    // 获取集合 ID
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

private:
    common::CollectionId collection_id_;
};

} // namespace litedb::core::physical_planner::plan