#pragma once

#include <memory>
#include <vector>

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/common/ids.hpp"
#include "core/physical_planner/plan/physical_plan.hpp"

namespace litedb::core::physical_planner::plan
{

// INSERT 语句物理计划
class InsertPlan final : public PhysicalPlan
{
public:
    InsertPlan(
        common::CollectionId collection_id,
        std::vector<std::unique_ptr<binder::bound::BoundExpression>> values
    );

public:
    // 获取集合 ID
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    // 获取插入值
    [[nodiscard]]
    const std::vector<std::unique_ptr<binder::bound::BoundExpression>> & values() const noexcept;

private:
    // 保留 collection_id_，减少后续执行时需要扫描算子树查找目标集合的开销
    common::CollectionId collection_id_;
    std::vector<std::unique_ptr<binder::bound::BoundExpression>> values_;
};

} // namespace litedb::core::physical_planner::plan
