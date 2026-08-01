#include "core/physical_planner/node/physical_sort.hpp"

#include <memory>
#include <utility>

namespace litedb::core::physical_plan
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

PhysicalSort::PhysicalSort(
    std::unique_ptr<PhysicalPlanNode> child,
    std::vector<binder::bound::BoundOrderByItem> order_by,
    parser::ast::AstNodeLocation location
)
    : PhysicalUnaryNode(PhysicalPlanNodeKind::Sort, std::move(child), location)
    , order_by_(std::move(order_by))
{
}

const std::vector<binder::bound::BoundOrderByItem> & PhysicalSort::order_by() const noexcept
{
    return order_by_;
}

std::unique_ptr<PhysicalPlanNode> PhysicalSort::clone() const
{
    return std::make_unique<PhysicalSort>(child().clone(), clone_order_by(order_by_), location());
}

} // namespace litedb::core::physical_plan
