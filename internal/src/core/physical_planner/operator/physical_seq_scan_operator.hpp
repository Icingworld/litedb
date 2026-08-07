#pragma once

#include "core/common/ids.hpp"
#include "core/physical_planner/operator/physical_operator.hpp"

namespace litedb::core::physical_planner::op
{

/**
 * @brief 顺序扫描算子
 * @details 叶子算子，没有子算子，直接继承自 PhysicalOperator
 */
class SeqScanOperator final : public PhysicalOperator
{
public:
    explicit SeqScanOperator(common::CollectionId collection_id) noexcept;

public:
    /**
     * @brief 获取集合 ID
     * @return 集合 ID
     */
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

private:
    common::CollectionId collection_id_;            // 集合 ID
};

} // namespace litedb::core::physical_planner::op
