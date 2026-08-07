#pragma once

#include <memory>
#include <vector>

#include "core/binder/bound/bound_order_by_item.hpp"
#include "core/logical_planner/operator/logical_unary_operator.hpp"

namespace litedb::core::logical_planner::op
{

/**
 * @brief 逻辑排序算子
 */
class LogicalOrderByOperator final : public LogicalUnaryOperator
{
public:
    LogicalOrderByOperator(
        std::unique_ptr<LogicalPlanOperator> child,
        std::vector<binder::bound::BoundOrderByItem> order_by
    );

public:
    /**
     * @brief 获取排序项
     * @return 排序项
     */
    [[nodiscard]]
    const std::vector<binder::bound::BoundOrderByItem> &
    order_by() const noexcept;

    /**
     * @brief 移出排序项
     * @return 排序项所有权
     */
    [[nodiscard]]
    std::vector<binder::bound::BoundOrderByItem>
    take_order_by() noexcept;

private:
    std::vector<binder::bound::BoundOrderByItem> order_by_;   // 排序项
};

} // namespace litedb::core::logical_planner::op
