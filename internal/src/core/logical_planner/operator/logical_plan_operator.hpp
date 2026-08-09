#pragma once

#include <cstdint>

namespace litedb::core::logical_planner::op
{

// 逻辑计划算子类型
enum class LogicalPlanOperatorKind : uint8_t
{
    Scan,
    Filter,
    Projection,
    OrderBy,
    Limit,
};

// 逻辑计划算子
class LogicalPlanOperator
{
public:
    LogicalPlanOperator(const LogicalPlanOperator &) = delete;

    LogicalPlanOperator & operator=(const LogicalPlanOperator &) = delete;

    LogicalPlanOperator(LogicalPlanOperator &&) noexcept = default;

    LogicalPlanOperator & operator=(LogicalPlanOperator &&) noexcept = default;

    virtual ~LogicalPlanOperator() noexcept = default;

protected:
    LogicalPlanOperator(LogicalPlanOperatorKind kind) noexcept;

public:
    // 获取算子类型
    [[nodiscard]]
    LogicalPlanOperatorKind kind() const noexcept;

private:
    LogicalPlanOperatorKind kind_;
};

} // namespace litedb::core::logical_planner::op
