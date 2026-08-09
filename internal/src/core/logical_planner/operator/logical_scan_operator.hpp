#pragma once

#include "core/common/ids.hpp"
#include "core/logical_planner/operator/logical_plan_operator.hpp"

namespace litedb::core::logical_planner::op
{

// 逻辑扫描算子
// 叶子算子，没有子算子，直接继承自 LogicalPlanOperator
// 该算子的含义是扫描一个集合
class LogicalScanOperator final : public LogicalPlanOperator
{
public:
    LogicalScanOperator(common::CollectionId collection_id);

public:
    // 获取集合 ID
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

private:
    common::CollectionId collection_id_;
};

} // namespace litedb::core::logical_planner::op
