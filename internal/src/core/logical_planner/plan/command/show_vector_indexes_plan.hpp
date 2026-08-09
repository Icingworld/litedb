#pragma once

#include "core/common/ids.hpp"
#include "core/logical_planner/plan/logical_plan.hpp"

namespace litedb::core::logical_planner::plan
{

// SHOW VINDEXES 语句逻辑计划
class ShowVectorIndexesPlan final : public LogicalPlan
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

} // namespace litedb::core::logical_planner::plan
