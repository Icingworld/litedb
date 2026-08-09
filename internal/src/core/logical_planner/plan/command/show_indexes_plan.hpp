#pragma once

#include "core/common/ids.hpp"
#include "core/logical_planner/plan/logical_plan.hpp"

namespace litedb::core::logical_planner::plan
{

// SHOW INDEXES 语句逻辑计划
class ShowIndexesPlan final : public LogicalPlan
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

} // namespace litedb::core::logical_planner::plan
