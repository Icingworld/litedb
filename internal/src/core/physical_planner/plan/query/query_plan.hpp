#pragma once

#include <memory>
#include <utility>

#include "core/physical_planner/operator/physical_operator.hpp"
#include "core/physical_planner/plan/physical_plan.hpp"

namespace litedb::core::physical_planner::plan
{

class QueryPlan final : public PhysicalPlan
{
public:
    explicit QueryPlan(std::unique_ptr<op::PhysicalOperator> root)
        : PhysicalPlan(PhysicalPlanKind::Query)
        , root_(std::move(root))
    {
    }

    [[nodiscard]] const op::PhysicalOperator & root() const noexcept
    {
        return *root_;
    }

    [[nodiscard]] const op::PhysicalOperator * root_ptr() const noexcept
    {
        return root_.get();
    }

private:
    std::unique_ptr<op::PhysicalOperator> root_;
};

} // namespace litedb::core::physical_planner::plan
