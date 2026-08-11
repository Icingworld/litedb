#pragma once

#include <memory>

#include "core/common/ids.hpp"
#include "core/physical_planner/operator/physical_operator.hpp"
#include "core/physical_planner/plan/physical_plan.hpp"

namespace litedb::core::physical_planner::plan
{

// DELETE 语句物理计划
class DeletePlan final : public PhysicalPlan
{
public:
    DeletePlan(
        common::CollectionId collection_id,
        std::unique_ptr<op::PhysicalOperator> root_operator
    );

public:
    // 获取集合 ID
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    // 获取根算子
    [[nodiscard]]
    const op::PhysicalOperator & root_operator() const noexcept;

private:
    // 保留 collection_id_，减少后续执行时需要扫描算子树查找目标集合的开销
    common::CollectionId collection_id_;
    std::unique_ptr<op::PhysicalOperator> root_operator_;
};

} // namespace litedb::core::physical_planner::plan
