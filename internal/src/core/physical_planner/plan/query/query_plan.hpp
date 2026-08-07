#pragma once

#include <memory>

#include "core/physical_planner/operator/physical_operator.hpp"
#include "core/physical_planner/plan/physical_plan.hpp"

namespace litedb::core::physical_planner::plan
{

/**
 * @brief QUERY 语句计划
 */
class QueryPlan final : public PhysicalPlan
{
public:
    explicit QueryPlan(std::unique_ptr<op::PhysicalOperator> root_operator);

public:
    /**
     * @brief 获取根算子
     * @return 根算子
     */
    [[nodiscard]]
    const op::PhysicalOperator & root_operator() const noexcept;

private:
    std::unique_ptr<op::PhysicalOperator> root_operator_;   // 根算子
};

} // namespace litedb::core::physical_planner::plan
