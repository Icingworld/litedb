#pragma once

#include <memory>

#include "core/logical_planner/operator/logical_plan_operator.hpp"
#include "core/logical_planner/plan/logical_plan.hpp"

namespace litedb::core::logical_planner::plan
{

/**
 * @brief QUERY 语句计划
 */
class QueryPlan final : public LogicalPlan
{
public:
    explicit QueryPlan(std::unique_ptr<op::LogicalPlanOperator> root_operator);

public:
    /**
     * @brief 获取根算子
     * @return 根算子
     */
    [[nodiscard]]
    const op::LogicalPlanOperator & root_operator() const noexcept;

private:
    std::unique_ptr<op::LogicalPlanOperator> root_operator_;   ///< 根算子
};

} // namespace litedb::core::logical_planner::plan
