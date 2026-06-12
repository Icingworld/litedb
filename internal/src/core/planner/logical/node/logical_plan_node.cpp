#include "core/planner/logical/node/logical_plan_node.hpp"

namespace litedb::core::planner::logical
{

LogicalPlanNode::LogicalPlanNode(LogicalPlanNodeKind kind, parser::ast::AstNodeLocation location) noexcept
    : kind_(kind)
    , location_(location)
{
}

LogicalPlanNodeKind LogicalPlanNode::kind() const noexcept
{
    return kind_;
}

parser::ast::AstNodeLocation LogicalPlanNode::location() const noexcept
{
    return location_;
}

} // namespace litedb::core::planner::logical
