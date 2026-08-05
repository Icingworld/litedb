#pragma once

#include <memory>

#include "core/physical_planner/operator/physical_operator.hpp"

namespace litedb::core::physical_planner::op
{

/**
 * @brief 物理一元算子
 * @details 只有一个子算子的算子
 */
class PhysicalUnaryOperator : public PhysicalOperator
{
protected:
    PhysicalUnaryOperator(
        PhysicalOperatorKind kind,
        std::unique_ptr<PhysicalOperator> child
    ) noexcept;

public:
    /**
     * @brief 获取子算子
     * @return 子算子
     */
    [[nodiscard]]
    const PhysicalOperator & child() const noexcept;

private:
    std::unique_ptr<PhysicalOperator> child_;   ///< 子算子
};

} // namespace litedb::core::physical_planner::op
