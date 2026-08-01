#include "core/physical_planner/node/physical_limit.hpp"

#include <memory>
#include <utility>

namespace litedb::core::physical_plan
{

PhysicalLimit::PhysicalLimit(
    std::unique_ptr<PhysicalPlanNode> child,
    std::optional<std::size_t> limit,
    std::optional<std::size_t> offset,
    parser::ast::AstNodeLocation location
)
    : PhysicalUnaryNode(PhysicalPlanNodeKind::Limit, std::move(child), location)
    , limit_(limit)
    , offset_(offset)
{
}

std::optional<std::size_t> PhysicalLimit::limit() const noexcept
{
    return limit_;
}

std::optional<std::size_t> PhysicalLimit::offset() const noexcept
{
    return offset_;
}

std::unique_ptr<PhysicalPlanNode> PhysicalLimit::clone() const
{
    return std::make_unique<PhysicalLimit>(child().clone(), limit_, offset_, location());
}

} // namespace litedb::core::physical_plan
