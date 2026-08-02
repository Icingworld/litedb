#pragma once

#include <memory>

#include "core/logical_planner/operator/logical_plan_operator.hpp"

namespace litedb::core::logical_planner::op
{

/**
 * @brief 逻辑一元算子
 * @details 只有一个子算子的算子
 */
class LogicalUnaryOperator : public LogicalPlanOperator
{
protected:
    LogicalUnaryOperator(
        LogicalPlanOperatorKind kind,
        std::unique_ptr<LogicalPlanOperator> child
    ) noexcept;

public:
    /**
     * @brief 获取子算子
     * @return 子算子
     */
    [[nodiscard]]
    const LogicalPlanOperator & child() const noexcept;

private:
    std::unique_ptr<LogicalPlanOperator> child_;   ///< 子算子
};

} // namespace litedb::core::logical_planner::op
