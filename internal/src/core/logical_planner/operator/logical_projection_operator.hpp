#pragma once

#include <memory>
#include <vector>

#include "core/binder/bound/bound_projection_item.hpp"
#include "core/logical_planner/operator/logical_unary_operator.hpp"

namespace litedb::core::logical_planner::op
{

// 逻辑投影算子
class LogicalProjectionOperator final : public LogicalUnaryOperator
{
public:
    LogicalProjectionOperator(
        std::unique_ptr<LogicalPlanOperator> child,
        std::vector<binder::bound::BoundProjectionItem> projections
    );

public:
    // 获取投影项
    [[nodiscard]]
    const std::vector<binder::bound::BoundProjectionItem> & projections() const noexcept;

    // 获取投影项所有权
    [[nodiscard]]
    std::vector<binder::bound::BoundProjectionItem> take_projections() noexcept;

private:
    std::vector<binder::bound::BoundProjectionItem> projections_;
};

} // namespace litedb::core::logical_planner::op
