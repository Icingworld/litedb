#pragma once

#include <memory>
#include <vector>

#include "core/logical_planner/plan/logical_plan.hpp"
#include "core/common/ids.hpp"
#include "core/binder/bound/expression/bound_expression.hpp"

namespace litedb::core::logical_planner::plan
{

// INSERT 语句逻辑计划
class InsertPlan final : public LogicalPlan
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

    // 获取值
    [[nodiscard]]
    const std::vector<std::unique_ptr<binder::bound::BoundExpression>> &
    values() const noexcept;

    // 获取插入值所有权
    [[nodiscard]]
    std::vector<std::unique_ptr<binder::bound::BoundExpression>>
    take_values() noexcept;

private:
    // 保留 collection_id_，减少后续执行时需要扫描算子树查找目标集合的开销
    common::CollectionId collection_id_;
    std::vector<std::unique_ptr<binder::bound::BoundExpression>> values_;
};

} // namespace litedb::core::logical_planner::plan
