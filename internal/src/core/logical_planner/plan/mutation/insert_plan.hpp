#pragma once

#include <memory>
#include <vector>

#include "core/logical_planner/plan/logical_plan.hpp"
#include "core/common/ids.hpp"
#include "core/binder/bound/expression/bound_expression.hpp"

namespace litedb::core::logical_planner::plan
{

/**
 * @brief INSERT 语句计划
 */
class InsertPlan final : public LogicalPlan
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
     * @brief 获取值
     * @return 值
     */
    [[nodiscard]]
    const std::vector<std::unique_ptr<binder::bound::BoundExpression>> &
    values() const noexcept;

    /**
     * @brief 移出插入值
     * @return 插入值所有权
     */
    [[nodiscard]]
    std::vector<std::unique_ptr<binder::bound::BoundExpression>>
    take_values() noexcept;

private:
    common::CollectionId collection_id_;                                        ///< 集合 ID
    std::vector<std::unique_ptr<binder::bound::BoundExpression>> values_;       ///< 值
};

} // namespace litedb::core::logical_planner::plan
