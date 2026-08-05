#pragma once

#include <memory>
#include <vector>
#include <utility>

#include "core/binder/bound/bound_projection_item.hpp"
#include "core/physical_planner/operator/physical_unary_operator.hpp"

namespace litedb::core::physical_planner::op
{

class ProjectionOperator final : public PhysicalUnaryOperator
{
public:
    ProjectionOperator(
        std::unique_ptr<PhysicalOperator> child,
        std::vector<binder::bound::BoundProjectionItem> projections
    ) noexcept
        : PhysicalUnaryOperator(PhysicalOperatorKind::Projection, std::move(child))
        , projections_(std::move(projections))
    {
    }

    [[nodiscard]] const std::vector<binder::bound::BoundProjectionItem> &
    projections() const noexcept
    {
        return projections_;
    }

private:
    std::vector<binder::bound::BoundProjectionItem> projections_;
};

} // namespace litedb::core::physical_planner::op
