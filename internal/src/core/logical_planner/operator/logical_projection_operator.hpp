#pragma once

#include <memory>
#include <vector>

#include "core/binder/bound/statement/bound_select_statement.hpp"
#include "core/logical_planner/operator/logical_unary_operator.hpp"

namespace litedb::core::logical_planner::op
{

/**
 * @brief 逻辑投影算子
 */
class LogicalProjectionOperator final : public LogicalUnaryOperator
{
public:
    LogicalProjectionOperator(
        std::unique_ptr<LogicalPlanOperator> child,
        std::vector<binder::bound::BoundProjectionItem> projections
    );

public:
    /**
     * @brief 获取投影项
     * @return 投影项
     */
    [[nodiscard]]
    const std::vector<binder::bound::BoundProjectionItem> &
    projections() const noexcept;

    /**
     * @brief 移出投影项
     * @return 投影项所有权
     */
    [[nodiscard]]
    std::vector<binder::bound::BoundProjectionItem>
    take_projections() noexcept;

private:
    std::vector<binder::bound::BoundProjectionItem> projections_;   ///< 投影项
};

} // namespace litedb::core::logical_planner::op
