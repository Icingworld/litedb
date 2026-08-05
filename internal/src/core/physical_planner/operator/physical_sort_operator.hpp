#pragma once

#include <memory>
#include <vector>
#include <utility>

#include "core/binder/bound/bound_order_by_item.hpp"
#include "core/physical_planner/operator/physical_unary_operator.hpp"

namespace litedb::core::physical_planner::op
{

class SortOperator final : public PhysicalUnaryOperator
{
public:
    SortOperator(
        std::unique_ptr<PhysicalOperator> child,
        std::vector<binder::bound::BoundOrderByItem> order_by
    ) noexcept
        : PhysicalUnaryOperator(PhysicalOperatorKind::Sort, std::move(child))
        , order_by_(std::move(order_by))
    {
    }

    [[nodiscard]] const std::vector<binder::bound::BoundOrderByItem> &
    order_by() const noexcept
    {
        return order_by_;
    }

private:
    std::vector<binder::bound::BoundOrderByItem> order_by_;
};

} // namespace litedb::core::physical_planner::op
