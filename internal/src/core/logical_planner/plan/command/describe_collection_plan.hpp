#pragma once

#include "core/logical_planner/plan/logical_plan.hpp"
#include "core/common/ids.hpp"

namespace litedb::core::logical_planner::plan
{

// DESCRIBE COLLECTION 语句逻辑计划
class DescribeCollectionPlan final : public LogicalPlan
{
public:
    explicit DescribeCollectionPlan(
        common::CollectionId collection_id
    ) noexcept;

public:
    // 获取集合 ID
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

private:
    common::CollectionId collection_id_;
};

} // namespace litedb::core::logical_planner::plan
