#include "core/physical_planner/operator/physical_limit_operator.hpp"

#include <utility>

namespace litedb::core::physical_planner::op
{

LimitOperator::LimitOperator(
    std::unique_ptr<PhysicalOperator> child,
    std::optional<std::size_t> limit,
    std::optional<std::size_t> offset
) noexcept
    : PhysicalUnaryOperator(PhysicalOperatorKind::Limit, std::move(child))
    , limit_(limit)
    , offset_(offset)
{
}

std::optional<std::size_t> LimitOperator::limit() const noexcept
{
    return limit_;
}

std::optional<std::size_t> LimitOperator::offset() const noexcept
{
    return offset_;
}

} // namespace litedb::core::physical_planner::op
