#pragma once

#include <memory>
#include <vector>

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/common/ids.hpp"
#include "core/physical_planner/plan/physical_plan.hpp"

namespace litedb::core::physical_planner::plan
{

/**
 * @brief INSERT 语句计划
 */
class InsertPlan final : public PhysicalPlan
{
public:
    InsertPlan(
        common::CollectionId collection_id,
        std::vector<std::unique_ptr<binder::bound::BoundExpression>> values
    );

public:
    /**
     * @brief 获取集合 ID
     * @return 集合 ID
     */
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    /**
     * @brief 获取插入值
     * @return 插入值
     */
    [[nodiscard]]
    const std::vector<std::unique_ptr<binder::bound::BoundExpression>> &
    values() const noexcept;

private:
    common::CollectionId collection_id_;                                        ///< 集合 ID
    std::vector<std::unique_ptr<binder::bound::BoundExpression>> values_;       ///< 插入值
};

} // namespace litedb::core::physical_planner::plan
