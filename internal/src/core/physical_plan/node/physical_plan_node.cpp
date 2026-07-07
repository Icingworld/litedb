#include "core/physical_plan/node/physical_plan_node.hpp"

namespace litedb::core::physical_plan
{

PhysicalPlanNode::PhysicalPlanNode(PhysicalPlanNodeKind kind, parser::ast::AstNodeLocation location) noexcept
    : kind_(kind)
    , location_(location)
{
}

PhysicalPlanNodeKind PhysicalPlanNode::kind() const noexcept
{
    return kind_;
}

parser::ast::AstNodeLocation PhysicalPlanNode::location() const noexcept
{
    return location_;
}

} // namespace litedb::core::physical_plan
