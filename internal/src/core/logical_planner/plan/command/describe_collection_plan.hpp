#pragma once

#include "core/logical_planner/plan/logical_plan.hpp"
#include "core/common/ids.hpp"

namespace litedb::core::logical_planner::plan
{

/**
 * @brief DESCRIBE COLLECTION 语句计划
 */
class DescribeCollectionPlan final : public LogicalPlan
{
public:
    explicit DescribeCollectionPlan(
        common::CollectionId collection_id
    ) noexcept;

public:
    /**
     * @brief 获取集合 ID
     * @return 集合 ID
     */
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

private:
    common::CollectionId collection_id_;    // 集合 ID
};

} // namespace litedb::core::logical_planner::plan
