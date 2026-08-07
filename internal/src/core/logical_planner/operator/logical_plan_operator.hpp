#pragma once

#include <cstdint>

namespace litedb::core::logical_planner::op
{

/**
 * @brief 逻辑计划算子类型
 */
enum class LogicalPlanOperatorKind : uint8_t
{
    Scan = 0,               // 扫描
    Filter = 1,             // 过滤
    Projection = 2,         // 投影
    OrderBy = 3,            // 排序
    Limit = 4,              // 限制
};

/**
 * @brief 逻辑计划算子
 */
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
    /**
     * @brief 获取算子类型
     * @return 算子类型
     */
    [[nodiscard]]
    LogicalPlanOperatorKind kind() const noexcept;

private:
    LogicalPlanOperatorKind kind_;                  // 算子类型
};

} // namespace litedb::core::logical_planner::op
