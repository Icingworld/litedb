#include "core/planner/logical/logical_filter.hpp"

#include <utility>

namespace litedb::core::planner::logical
{

LogicalFilter::LogicalFilter(
    std::unique_ptr<LogicalPlanNode> child,
    std::unique_ptr<binder::bound::BoundExpression> predicate,
    parser::ast::AstNodeLocation location
)
    : LogicalUnaryNode(LogicalPlanNodeKind::Filter, std::move(child), location),
      predicate_(std::move(predicate))
{
}

const binder::bound::BoundExpression & LogicalFilter::predicate() const noexcept { return *predicate_; }

} // namespace litedb::core::planner::logical
