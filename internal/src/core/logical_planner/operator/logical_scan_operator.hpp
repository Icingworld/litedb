#pragma once

#include "core/common/ids.hpp"
#include "core/logical_planner/operator/logical_plan_operator.hpp"

namespace litedb::core::logical_planner::op
{

/**
 * @brief 逻辑扫描算子
 * @details 叶子算子，没有子算子，直接继承自 LogicalPlanOperator
 */
class LogicalScanOperator final : public LogicalPlanOperator
{
public:
    LogicalScanOperator(common::CollectionId collection_id);

public:
    /**
     * @brief 获取集合ID
     * @return 集合ID
     */
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

private:
    common::CollectionId collection_id_;             ///< 集合 ID
};

} // namespace litedb::core::logical_planner::op
