#pragma once

#include <memory>

#include "core/common/ids.hpp"
#include "core/logical_planner/operator/logical_plan_operator.hpp"
#include "core/logical_planner/plan/logical_plan.hpp"

namespace litedb::core::logical_planner::plan
{

// DELETE 语句逻辑计划
class DeletePlan final : public LogicalPlan
{
public:
    DeletePlan(
        common::CollectionId collection_id,
        std::unique_ptr<op::LogicalPlanOperator> root_operator
    );

public:
    // 获取根算子
    [[nodiscard]]
    const op::LogicalPlanOperator & root_operator() const noexcept;

    // 获取根算子所有权
    // 调用后 root_operator() 不可调用；再次调用返回 nullptr
    [[nodiscard]]
    std::unique_ptr<op::LogicalPlanOperator> take_root_operator() noexcept;

    // 获取集合 ID
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

private:
    // 保留 collection_id_，减少后续执行时需要扫描算子树查找目标集合的开销
    common::CollectionId collection_id_;
    std::unique_ptr<op::LogicalPlanOperator> root_operator_;
};

} // namespace litedb::core::logical_planner::plan
