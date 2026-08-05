#pragma once

#include <memory>
#include <vector>
#include <utility>

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/common/ids.hpp"
#include "core/physical_planner/plan/physical_plan.hpp"

namespace litedb::core::physical_planner::plan
{

class InsertPlan final : public PhysicalPlan
{
public:
    InsertPlan(
        common::CollectionId collection_id,
        std::vector<std::unique_ptr<binder::bound::BoundExpression>> values
    )
        : PhysicalPlan(PhysicalPlanKind::Insert)
        , collection_id_(collection_id)
        , values_(std::move(values))
    {
    }

    [[nodiscard]] common::CollectionId collection_id() const noexcept { return collection_id_; }

    [[nodiscard]] const std::vector<std::unique_ptr<binder::bound::BoundExpression>> &
    values() const noexcept
    {
        return values_;
    }

private:
    common::CollectionId collection_id_;
    std::vector<std::unique_ptr<binder::bound::BoundExpression>> values_;
};

} // namespace litedb::core::physical_planner::plan
