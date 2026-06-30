#include "core/planner/logical/node/logical_projection.hpp"

#include <utility>

namespace litedb::core::planner::logical
{

LogicalProjection::LogicalProjection(
    std::unique_ptr<LogicalPlanNode> child,
    std::vector<binder::bound::BoundProjectionItem> projections,
    parser::ast::AstNodeLocation location
)
    : LogicalUnaryNode(LogicalPlanNodeKind::Projection, std::move(child), location)
    , projections_(std::move(projections))
{
}

const std::vector<binder::bound::BoundProjectionItem> & LogicalProjection::projections() const noexcept
{
    return projections_;
}

} // namespace litedb::core::planner::logical
