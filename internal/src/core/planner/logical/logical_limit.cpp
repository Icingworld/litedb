#include "core/planner/logical/logical_limit.hpp"

#include <utility>

namespace litedb::core::planner::logical
{

LogicalLimit::LogicalLimit(
    std::unique_ptr<LogicalPlanNode> child,
    std::optional<std::size_t> limit,
    std::optional<std::size_t> offset,
    parser::ast::AstNodeLocation location
)
    : LogicalUnaryNode(LogicalPlanNodeKind::Limit, std::move(child), location),
      limit_(limit),
      offset_(offset)
{
}

std::optional<std::size_t> LogicalLimit::limit() const noexcept { return limit_; }
std::optional<std::size_t> LogicalLimit::offset() const noexcept { return offset_; }

} // namespace litedb::core::planner::logical
