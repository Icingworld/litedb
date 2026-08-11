#include "core/physical_planner/operator/physical_unary_operator.hpp"

#include <utility>
#include <cassert>

namespace litedb::core::physical_planner::op
{

PhysicalUnaryOperator::PhysicalUnaryOperator(
    PhysicalOperatorKind kind,
    std::unique_ptr<PhysicalOperator> child
) noexcept
    : PhysicalOperator(kind)
    , child_(std::move(child))
{
    assert(child_ != nullptr);
}

const PhysicalOperator & PhysicalUnaryOperator::child() const noexcept
{
    return *child_;
}

} // namespace litedb::core::physical_planner::op
