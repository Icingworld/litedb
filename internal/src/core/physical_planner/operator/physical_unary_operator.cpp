#include "core/physical_planner/operator/physical_unary_operator.hpp"

#include <utility>

namespace litedb::core::physical_planner::op
{

PhysicalUnaryOperator::PhysicalUnaryOperator(
    PhysicalOperatorKind kind,
    std::unique_ptr<PhysicalOperator> child
) noexcept
    : PhysicalOperator(kind)
    , child_(std::move(child))
{
}

const PhysicalOperator & PhysicalUnaryOperator::child() const noexcept
{
    return *child_;
}

} // namespace litedb::core::physical_planner::op
