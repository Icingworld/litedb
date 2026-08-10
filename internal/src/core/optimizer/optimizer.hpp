#pragma once

#include <memory>

namespace litedb::core::logical_planner::plan
{

class LogicalPlan;

} // namespace litedb::core::logical_planner::plan

namespace litedb::core::optimizer
{

// 优化器选项
struct OptimizerOptions
{
    bool enabled {true}; // 是否启用优化器
};

// 优化器
class Optimizer final
{
public:
    explicit Optimizer(OptimizerOptions options = {}) noexcept;

public:
    // 优化逻辑计划
    [[nodiscard]]
    std::unique_ptr<logical_planner::plan::LogicalPlan> optimize(
        std::unique_ptr<logical_planner::plan::LogicalPlan> plan
    );

private:
    OptimizerOptions options_;
};

} // namespace litedb::core::optimizer
