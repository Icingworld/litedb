#pragma once

#include <memory>

#include "core/logical_planner/operator/logical_plan_operator.hpp"
#include "core/logical_planner/plan/logical_plan.hpp"
#include "core/common/ids.hpp"

namespace litedb::core::logical_planner::plan
{

/**
 * @brief DELETE 语句计划
 */
class DeletePlan final : public LogicalPlan
{
public:
    DeletePlan(
        common::CollectionId collection_id,
        std::unique_ptr<op::LogicalPlanOperator> root_operator
    );

public:
    /**
     * @brief 获取根算子
     * @return 根算子
     */
    [[nodiscard]]
    const op::LogicalPlanOperator & root_operator() const noexcept;

    /**
     * @brief 移出根算子
     * @return 根算子所有权
     * @warning 调用后不可再调用 root_operator()
     */
    [[nodiscard]]
    std::unique_ptr<op::LogicalPlanOperator> take_root_operator() noexcept;

    /**
     * @brief 获取集合 ID
     * @return 集合 ID
     */
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

private:
    // 保留 collection_id_，减少后续执行时需要扫描算子树查找目标集合的开销
    common::CollectionId collection_id_;                        ///< 集合 ID
    std::unique_ptr<op::LogicalPlanOperator> root_operator_;    ///< 根算子
};

} // namespace litedb::core::logical_planner::plan
