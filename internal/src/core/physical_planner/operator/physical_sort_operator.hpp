#pragma once

#include <memory>
#include <vector>

#include "core/binder/bound/bound_order_by_item.hpp"
#include "core/physical_planner/operator/physical_unary_operator.hpp"

namespace litedb::core::physical_planner::op
{

// 排序算子
class SortOperator final : public PhysicalUnaryOperator
{
public:
    SortOperator(
        std::unique_ptr<PhysicalOperator> child,
        std::vector<binder::bound::BoundOrderByItem> order_by
    ) noexcept;

public:
    // 获取排序项
    [[nodiscard]]
    const std::vector<binder::bound::BoundOrderByItem> &
    order_by() const noexcept;

private:
    std::vector<binder::bound::BoundOrderByItem> order_by_;
};

} // namespace litedb::core::physical_planner::op
