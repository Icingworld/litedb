#include "core/planner/logical/logical_projection.hpp"

#include <utility>

namespace litedb::core::planner::logical
{

LogicalProjection::LogicalProjection(
    std::unique_ptr<LogicalPlanNode> child,
    std::vector<std::unique_ptr<binder::bound::BoundExpression>> projections,
    parser::ast::AstNodeLocation location
)
    : LogicalUnaryNode(LogicalPlanNodeKind::Projection, std::move(child), location),
      projections_(std::move(projections))
{
}

const std::vector<std::unique_ptr<binder::bound::BoundExpression>> & LogicalProjection::projections() const noexcept
{
    return projections_;
}

} // namespace litedb::core::planner::logical
