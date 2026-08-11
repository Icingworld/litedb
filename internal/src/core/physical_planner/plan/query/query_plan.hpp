#pragma once

#include <memory>

#include "core/physical_planner/operator/physical_operator.hpp"
#include "core/physical_planner/plan/physical_plan.hpp"

namespace litedb::core::physical_planner::plan
{

// QUERY 语句物理计划
class QueryPlan final : public PhysicalPlan
{
public:
    explicit QueryPlan(std::unique_ptr<op::PhysicalOperator> root_operator);

public:
    // 获取根算子
    [[nodiscard]]
    const op::PhysicalOperator & root_operator() const noexcept;

private:
    std::unique_ptr<op::PhysicalOperator> root_operator_;
};

} // namespace litedb::core::physical_planner::plan
