#pragma once

#include <memory>

#include "core/physical_planner/operator/physical_operator.hpp"

namespace litedb::core::physical_planner::op
{

class PhysicalUnaryOperator : public PhysicalOperator
{
protected:
    PhysicalUnaryOperator(
        PhysicalOperatorKind kind,
        std::unique_ptr<PhysicalOperator> child
    ) noexcept
        : PhysicalOperator(kind)
        , child_(std::move(child))
    {
    }

public:
    [[nodiscard]] const PhysicalOperator & child() const noexcept
    {
        return *child_;
    }

    [[nodiscard]] const PhysicalOperator * child_ptr() const noexcept
    {
        return child_.get();
    }

private:
    std::unique_ptr<PhysicalOperator> child_;
};

} // namespace litedb::core::physical_planner::op
