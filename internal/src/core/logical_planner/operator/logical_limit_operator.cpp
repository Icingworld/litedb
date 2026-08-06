#include "core/logical_planner/operator/logical_limit_operator.hpp"

#include <utility>

namespace litedb::core::logical_planner::op
{

LogicalLimitOperator::LogicalLimitOperator(
    std::unique_ptr<LogicalPlanOperator> child,
    std::optional<std::size_t> limit,
    std::optional<std::size_t> offset
)
    : LogicalUnaryOperator(LogicalPlanOperatorKind::Limit, std::move(child))
    , limit_(limit)
    , offset_(offset)
{
}

std::optional<std::size_t> LogicalLimitOperator::limit() const noexcept
{
    return limit_;
}

std::optional<std::size_t> LogicalLimitOperator::offset() const noexcept
{
    return offset_;
}

} // namespace litedb::core::logical_planner::op
