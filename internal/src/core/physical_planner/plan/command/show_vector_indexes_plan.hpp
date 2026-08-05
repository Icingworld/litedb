#pragma once

#include "core/common/ids.hpp"
#include "core/physical_planner/plan/physical_plan.hpp"

namespace litedb::core::physical_planner::plan
{

/**
 * @brief SHOW VINDEXES 语句计划
 */
class ShowVectorIndexesPlan final : public PhysicalPlan
{
public:
    explicit ShowVectorIndexesPlan(common::CollectionId collection_id) noexcept;

public:
    /**
     * @brief 获取集合 ID
     * @return 集合 ID
     */
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

private:
    common::CollectionId collection_id_;         ///< 集合 ID
};

} // namespace litedb::core::physical_planner::plan