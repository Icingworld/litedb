#include "core/physical_planner/node/physical_unary_node.hpp"

#include <cassert>
#include <utility>

namespace litedb::core::physical_plan
{

PhysicalUnaryNode::PhysicalUnaryNode(
    PhysicalPlanNodeKind kind,
    std::unique_ptr<PhysicalPlanNode> child,
    parser::ast::AstNodeLocation location
) noexcept
    : PhysicalPlanNode(kind, location)
    , child_(std::move(child))
{
}

const PhysicalPlanNode & PhysicalUnaryNode::child() const noexcept
{
    assert(child_ != nullptr);
    return *child_;
}

} // namespace litedb::core::physical_plan
