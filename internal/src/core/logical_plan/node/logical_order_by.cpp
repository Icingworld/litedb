#include "core/logical_plan/node/logical_order_by.hpp"

#include <memory>
#include <utility>

namespace litedb::core::planner::logical
{

namespace
{

std::vector<binder::bound::BoundOrderByItem> clone_order_by(
    const std::vector<binder::bound::BoundOrderByItem> & order_by
)
{
    std::vector<binder::bound::BoundOrderByItem> cloned;
    cloned.reserve(order_by.size());
    for (const auto & item : order_by) {
        cloned.push_back(binder::bound::BoundOrderByItem {
            .expression = item.expression->clone(),
            .ascending = item.ascending,
        });
    }
    return cloned;
}

} // namespace

LogicalOrderBy::LogicalOrderBy(
    std::unique_ptr<LogicalPlanNode> child,
    std::vector<binder::bound::BoundOrderByItem> order_by,
    parser::ast::AstNodeLocation location
)
    : LogicalUnaryNode(LogicalPlanNodeKind::OrderBy, std::move(child), location)
    , order_by_(std::move(order_by))
{
}

const std::vector<binder::bound::BoundOrderByItem> & LogicalOrderBy::order_by() const noexcept
{
    return order_by_;
}

void LogicalOrderBy::accept(LogicalPlanNodeVisitor & visitor) const
{
    visitor.visit(*this);
}

std::unique_ptr<LogicalPlanNode> LogicalOrderBy::clone() const
{
    return std::make_unique<LogicalOrderBy>(child().clone(), clone_order_by(order_by_), location());
}

} // namespace litedb::core::planner::logical
