#include "core/planner/logical/node/logical_projection.hpp"

#include <memory>
#include <utility>

namespace litedb::core::planner::logical
{

namespace
{

std::vector<binder::bound::BoundProjectionItem> clone_projections(
    const std::vector<binder::bound::BoundProjectionItem> & projections
)
{
    std::vector<binder::bound::BoundProjectionItem> cloned;
    cloned.reserve(projections.size());
    for (const auto & projection : projections) {
        cloned.push_back(binder::bound::BoundProjectionItem {
            .expression = projection.expression->clone(),
            .alias = projection.alias,
        });
    }
    return cloned;
}

} // namespace

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

void LogicalProjection::accept(LogicalPlanNodeVisitor & visitor) const
{
    visitor.visit(*this);
}

std::unique_ptr<LogicalPlanNode> LogicalProjection::clone() const
{
    return std::make_unique<LogicalProjection>(child().clone(), clone_projections(projections_), location());
}

} // namespace litedb::core::planner::logical
