#pragma once

#include <memory>

#include "core/logical_planner/operator/dispatcher/logical_operator_dispatcher.hpp"
#include "core/physical_planner/access/scalar_access_path_selector.hpp"
#include "core/physical_planner/access/vector_top_k_selector.hpp"

namespace litedb::core::physical_planner::op
{

class PhysicalOperator;

} // namespace litedb::core::physical_planner::op

namespace litedb::core::physical_planner
{

// 逻辑算子到物理算子的递归工作器
class PhysicalOperatorWorker final
    : private logical_planner::op::MutableLogicalOperatorDispatcher<
          PhysicalOperatorWorker,
          std::unique_ptr<op::PhysicalOperator>
      >
{
    friend logical_planner::op::MutableLogicalOperatorDispatcher<
        PhysicalOperatorWorker,
        std::unique_ptr<op::PhysicalOperator>
    >;

public:
    explicit PhysicalOperatorWorker(const PhysicalPlannerContext & context) noexcept;

public:
    // 降级逻辑算子
    [[nodiscard]]
    std::unique_ptr<op::PhysicalOperator> lower_operator(
        std::unique_ptr<logical_planner::op::LogicalPlanOperator> logical_operator
    );

private:
    // 访问 SCAN 算子
    [[nodiscard]]
    std::unique_ptr<op::PhysicalOperator> visit_scan_operator(
        logical_planner::op::LogicalScanOperator & logical_operator
    );

    // 访问 FILTER 算子
    [[nodiscard]]
    std::unique_ptr<op::PhysicalOperator> visit_filter_operator(
        logical_planner::op::LogicalFilterOperator & logical_operator
    );

    // 访问 PROJECTION 算子
    [[nodiscard]]
    std::unique_ptr<op::PhysicalOperator> visit_projection_operator(
        logical_planner::op::LogicalProjectionOperator & logical_operator
    );

    // 访问 ORDER BY 算子
    [[nodiscard]]
    std::unique_ptr<op::PhysicalOperator> visit_order_by_operator(
        logical_planner::op::LogicalOrderByOperator & logical_operator
    );

    // 访问 LIMIT 算子
    [[nodiscard]]
    std::unique_ptr<op::PhysicalOperator> visit_limit_operator(
        logical_planner::op::LogicalLimitOperator & logical_operator
    );

private:
    const PhysicalPlannerContext & context_;
    ScalarAccessPathSelector scalar_selector_;
    VectorTopKSelector vector_selector_;
};

} // namespace litedb::core::physical_planner
