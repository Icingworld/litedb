#pragma once

#include <memory>

#include "core/logical_planner/operator/logical_plan_operator.hpp"
#include "core/logical_planner/plan/logical_plan.hpp"

namespace litedb::core::logical_planner::plan
{

// QUERY 语句逻辑计划
class QueryPlan final : public LogicalPlan
{
public:
    explicit QueryPlan(std::unique_ptr<op::LogicalPlanOperator> root_operator);

public:
    // 获取根算子
    [[nodiscard]]
    const op::LogicalPlanOperator & root_operator() const noexcept;

    // 获取根算子所有权
    [[nodiscard]]
    std::unique_ptr<op::LogicalPlanOperator> take_root_operator() noexcept;

private:
    std::unique_ptr<op::LogicalPlanOperator> root_operator_;
};

} // namespace litedb::core::logical_planner::plan
