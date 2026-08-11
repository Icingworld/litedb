#include "core/physical_planner/operator/physical_operator.hpp"

namespace litedb::core::physical_planner::op
{

PhysicalOperator::PhysicalOperator(PhysicalOperatorKind kind) noexcept
    : kind_(kind)
{}

PhysicalOperatorKind PhysicalOperator::kind() const noexcept
{
    return kind_;
}

} // namespace litedb::core::physical_planner::op
