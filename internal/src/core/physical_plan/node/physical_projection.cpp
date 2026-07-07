#include "core/physical_plan/node/physical_projection.hpp"

#include <memory>
#include <utility>

namespace litedb::core::physical_plan
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

PhysicalProjection::PhysicalProjection(
    std::unique_ptr<PhysicalPlanNode> child,
    std::vector<binder::bound::BoundProjectionItem> projections,
    parser::ast::AstNodeLocation location
)
    : PhysicalUnaryNode(PhysicalPlanNodeKind::Projection, std::move(child), location)
    , projections_(std::move(projections))
{
}

const std::vector<binder::bound::BoundProjectionItem> & PhysicalProjection::projections() const noexcept
{
    return projections_;
}

std::unique_ptr<PhysicalPlanNode> PhysicalProjection::clone() const
{
    return std::make_unique<PhysicalProjection>(child().clone(), clone_projections(projections_), location());
}

} // namespace litedb::core::physical_plan
