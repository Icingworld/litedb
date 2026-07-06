#include "core/planner/logical/node/logical_filter.hpp"

#include <cassert>
#include <memory>
#include <utility>

namespace litedb::core::planner::logical
{

LogicalFilter::LogicalFilter(
    std::unique_ptr<LogicalPlanNode> child,
    std::unique_ptr<binder::bound::BoundExpression> predicate,
    parser::ast::AstNodeLocation location
)
    : LogicalUnaryNode(LogicalPlanNodeKind::Filter, std::move(child), location)
    , predicate_(std::move(predicate))
{
}

const binder::bound::BoundExpression & LogicalFilter::predicate() const noexcept
{
    assert(predicate_ != nullptr);
    return *predicate_;
}

void LogicalFilter::accept(LogicalPlanNodeVisitor & visitor) const
{
    visitor.visit(*this);
}

std::unique_ptr<LogicalPlanNode> LogicalFilter::clone() const
{
    return std::make_unique<LogicalFilter>(child().clone(), predicate_->clone(), location());
}

} // namespace litedb::core::planner::logical
