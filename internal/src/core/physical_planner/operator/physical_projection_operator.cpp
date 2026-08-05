#include "core/physical_planner/operator/physical_projection_operator.hpp"

#include <utility>

namespace litedb::core::physical_planner::op
{

ProjectionOperator::ProjectionOperator(
    std::unique_ptr<PhysicalOperator> child,
    std::vector<binder::bound::BoundProjectionItem> projections
) noexcept
    : PhysicalUnaryOperator(PhysicalOperatorKind::Projection, std::move(child))
    , projections_(std::move(projections))
{
}

const std::vector<binder::bound::BoundProjectionItem> &
ProjectionOperator::projections() const noexcept
{
    return projections_;
}

} // namespace litedb::core::physical_planner::op
