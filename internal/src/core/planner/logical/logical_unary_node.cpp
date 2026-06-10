#include "core/planner/logical/logical_unary_node.hpp"

#include <utility>

namespace litedb::core::planner::logical
{

LogicalUnaryNode::LogicalUnaryNode(
    LogicalPlanNodeKind kind,
    std::unique_ptr<LogicalPlanNode> child,
    parser::ast::AstNodeLocation location
) noexcept
    : LogicalPlanNode(kind, location),
      child_(std::move(child))
{
}

const LogicalPlanNode & LogicalUnaryNode::child() const noexcept { return *child_; }

} // namespace litedb::core::planner::logical
