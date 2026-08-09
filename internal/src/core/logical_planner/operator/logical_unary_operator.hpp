#pragma once

#include <memory>

#include "core/logical_planner/operator/logical_plan_operator.hpp"

namespace litedb::core::logical_planner::op
{

// 逻辑一元算子
class LogicalUnaryOperator : public LogicalPlanOperator
{
protected:
    LogicalUnaryOperator(
        LogicalPlanOperatorKind kind,
        std::unique_ptr<LogicalPlanOperator> child
    ) noexcept;

public:
    // 获取子算子
    [[nodiscard]]
    const LogicalPlanOperator & child() const noexcept;

    // 获取子算子所有权
    [[nodiscard]]
    std::unique_ptr<LogicalPlanOperator> take_child() noexcept;

private:
    std::unique_ptr<LogicalPlanOperator> child_;
};

} // namespace litedb::core::logical_planner::op
