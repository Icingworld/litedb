#pragma once

#include <memory>
#include <utility>

#include "core/common/ids.hpp"
#include "core/physical_planner/operator/physical_operator.hpp"
#include "core/physical_planner/plan/physical_plan.hpp"

namespace litedb::core::physical_planner::plan
{

class DeletePlan final : public PhysicalPlan
{
public:
    DeletePlan(
        common::CollectionId collection_id,
        std::unique_ptr<op::PhysicalOperator> input
    )
        : PhysicalPlan(PhysicalPlanKind::Delete)
        , collection_id_(collection_id)
        , input_(std::move(input))
    {
    }

    [[nodiscard]] common::CollectionId collection_id() const noexcept { return collection_id_; }
    [[nodiscard]] const op::PhysicalOperator & input() const noexcept { return *input_; }
    [[nodiscard]] const op::PhysicalOperator * input_ptr() const noexcept { return input_.get(); }

private:
    common::CollectionId collection_id_;
    std::unique_ptr<op::PhysicalOperator> input_;
};

} // namespace litedb::core::physical_planner::plan
