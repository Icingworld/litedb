#pragma once

#include <memory>
#include <vector>

#include "core/binder/bound/bound_assignment.hpp"
#include "core/common/ids.hpp"
#include "core/physical_planner/operator/physical_operator.hpp"
#include "core/physical_planner/plan/physical_plan.hpp"

namespace litedb::core::physical_planner::plan
{

/**
 * @brief UPDATE 语句计划
 */
class UpdatePlan final : public PhysicalPlan
{
public:
    UpdatePlan(
        common::CollectionId collection_id,
        std::vector<binder::bound::BoundAssignment> assignments,
        std::unique_ptr<op::PhysicalOperator> root_operator
    );

public:
    /**
     * @brief 获取集合 ID
     * @return 集合 ID
     */
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    /**
     * @brief 获取赋值列表
     * @return 赋值列表
     */
    [[nodiscard]]
    const std::vector<binder::bound::BoundAssignment> &
    assignments() const noexcept;

    /**
     * @brief 获取根算子
     * @return 根算子
     */
    [[nodiscard]]
    const op::PhysicalOperator & root_operator() const noexcept;

private:
    // 保留 collection_id_，减少后续执行时需要扫描算子树查找目标集合的开销
    common::CollectionId collection_id_;                            ///< 集合 ID
    std::vector<binder::bound::BoundAssignment> assignments_;       ///< 赋值列表
    std::unique_ptr<op::PhysicalOperator> root_operator_;           ///< 根算子
};

} // namespace litedb::core::physical_planner::plan
