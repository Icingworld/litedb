#include "core/logical_planner/operator/logical_projection_operator.hpp"

#include <utility>

namespace litedb::core::logical_planner::op
{

LogicalProjectionOperator::LogicalProjectionOperator(
    std::unique_ptr<LogicalPlanOperator> child,
    std::vector<binder::bound::BoundProjectionItem> projections
)
    : LogicalUnaryOperator(LogicalPlanOperatorKind::Projection, std::move(child))
    , projections_(std::move(projections))
{}

const std::vector<binder::bound::BoundProjectionItem> &
LogicalProjectionOperator::projections() const noexcept
{
    return projections_;
}

std::vector<binder::bound::BoundProjectionItem>
LogicalProjectionOperator::take_projections() noexcept
{
    return std::move(projections_);
}

} // namespace litedb::core::logical_planner::op
