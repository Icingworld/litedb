#include "core/physical_planner/node/physical_filter.hpp"

#include <cassert>
#include <memory>
#include <utility>

namespace litedb::core::physical_plan
{

PhysicalFilter::PhysicalFilter(
    std::unique_ptr<PhysicalPlanNode> child,
    std::unique_ptr<binder::bound::BoundExpression> predicate,
    parser::ast::AstNodeLocation location
)
    : PhysicalUnaryNode(PhysicalPlanNodeKind::Filter, std::move(child), location)
    , predicate_(std::move(predicate))
{
}

const binder::bound::BoundExpression & PhysicalFilter::predicate() const noexcept
{
    assert(predicate_ != nullptr);
    return *predicate_;
}

std::unique_ptr<PhysicalPlanNode> PhysicalFilter::clone() const
{
    return std::make_unique<PhysicalFilter>(child().clone(), predicate_->clone(), location());
}

} // namespace litedb::core::physical_plan
