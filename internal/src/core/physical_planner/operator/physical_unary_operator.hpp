#pragma once

#include <memory>

#include "core/physical_planner/operator/physical_operator.hpp"

namespace litedb::core::physical_planner::op
{

// 物理一元算子
class PhysicalUnaryOperator : public PhysicalOperator
{
protected:
    PhysicalUnaryOperator(
        PhysicalOperatorKind kind,
        std::unique_ptr<PhysicalOperator> child
    ) noexcept;

public:
    // 获取子算子
    [[nodiscard]]
    const PhysicalOperator & child() const noexcept;

private:
    std::unique_ptr<PhysicalOperator> child_;
};

} // namespace litedb::core::physical_planner::op
