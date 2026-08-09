#pragma once

#include <cstddef>
#include <memory>
#include <optional>

#include "core/logical_planner/operator/logical_unary_operator.hpp"

namespace litedb::core::logical_planner::op
{

// 逻辑限制算子
class LogicalLimitOperator final : public LogicalUnaryOperator
{
public:
    LogicalLimitOperator(
        std::unique_ptr<LogicalPlanOperator> child,
        std::optional<std::size_t> limit,
        std::optional<std::size_t> offset
    );

public:
    // 获取限制
    [[nodiscard]]
    std::optional<std::size_t> limit() const noexcept;

    // 获取偏移
    [[nodiscard]]
    std::optional<std::size_t> offset() const noexcept;

private:
    std::optional<std::size_t> limit_;
    std::optional<std::size_t> offset_;
};

} // namespace litedb::core::logical_planner::op
