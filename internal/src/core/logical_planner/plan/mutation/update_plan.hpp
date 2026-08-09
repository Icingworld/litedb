#pragma once

#include <memory>
#include <vector>

#include "core/binder/bound/bound_assignment.hpp"
#include "core/common/ids.hpp"
#include "core/logical_planner/operator/logical_plan_operator.hpp"
#include "core/logical_planner/plan/logical_plan.hpp"

namespace litedb::core::logical_planner::plan
{

// UPDATE 语句逻辑计划
class UpdatePlan final : public LogicalPlan
{
public:
    UpdatePlan(
        common::CollectionId collection_id,
        std::vector<binder::bound::BoundAssignment> assignments,
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

    // 获取赋值
    [[nodiscard]]
    const std::vector<binder::bound::BoundAssignment> & assignments() const noexcept;

    // 获取赋值列表所有权
    // 调用后 assignments() 为空；再次调用返回空列表
    [[nodiscard]]
    std::vector<binder::bound::BoundAssignment> take_assignments() noexcept;

private:
    // 保留 collection_id_，减少后续执行时需要扫描算子树查找目标集合的开销
    common::CollectionId collection_id_;
    std::vector<binder::bound::BoundAssignment> assignments_;
    std::unique_ptr<op::LogicalPlanOperator> root_operator_;
};

} // namespace litedb::core::logical_planner::plan
