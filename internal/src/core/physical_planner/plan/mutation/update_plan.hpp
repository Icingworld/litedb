#pragma once

#include <memory>
#include <vector>
#include <utility>

#include "core/binder/bound/bound_assignment.hpp"
#include "core/common/ids.hpp"
#include "core/physical_planner/operator/physical_operator.hpp"
#include "core/physical_planner/plan/physical_plan.hpp"

namespace litedb::core::physical_planner::plan
{

class UpdatePlan final : public PhysicalPlan
{
public:
    UpdatePlan(
        common::CollectionId collection_id,
        std::vector<binder::bound::BoundAssignment> assignments,
        std::unique_ptr<op::PhysicalOperator> input
    )
        : PhysicalPlan(PhysicalPlanKind::Update)
        , collection_id_(collection_id)
        , assignments_(std::move(assignments))
        , input_(std::move(input))
    {
    }

    [[nodiscard]] common::CollectionId collection_id() const noexcept { return collection_id_; }
    [[nodiscard]] const std::vector<binder::bound::BoundAssignment> & assignments() const noexcept
    {
        return assignments_;
    }
    [[nodiscard]] const op::PhysicalOperator & input() const noexcept { return *input_; }
    [[nodiscard]] const op::PhysicalOperator * input_ptr() const noexcept { return input_.get(); }

private:
    common::CollectionId collection_id_;
    std::vector<binder::bound::BoundAssignment> assignments_;
    std::unique_ptr<op::PhysicalOperator> input_;
};

} // namespace litedb::core::physical_planner::plan
