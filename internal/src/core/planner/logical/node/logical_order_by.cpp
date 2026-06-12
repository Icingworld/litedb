#include "core/planner/logical/node/logical_order_by.hpp"

#include <utility>

namespace litedb::core::planner::logical
{

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

} // namespace litedb::core::planner::logical
