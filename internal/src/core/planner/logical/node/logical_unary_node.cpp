#include "core/planner/logical/node/logical_unary_node.hpp"

#include <utility>
#include <cassert>

namespace litedb::core::planner::logical
{

LogicalUnaryNode::LogicalUnaryNode(
    LogicalPlanNodeKind kind,
    std::unique_ptr<LogicalPlanNode> child,
    parser::ast::AstNodeLocation location
) noexcept
    : LogicalPlanNode(kind, location)
    , child_(std::move(child))
{
}

const LogicalPlanNode & LogicalUnaryNode::child() const noexcept
{
    assert(child_ != nullptr);
    return *child_;
}

} // namespace litedb::core::planner::logical
