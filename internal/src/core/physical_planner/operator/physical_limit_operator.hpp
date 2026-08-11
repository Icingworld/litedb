#pragma once

#include <cstddef>
#include <memory>
#include <optional>

#include "core/physical_planner/operator/physical_unary_operator.hpp"

namespace litedb::core::physical_planner::op
{

// 限制算子
class LimitOperator final : public PhysicalUnaryOperator
{
public:
    LimitOperator(
        std::unique_ptr<PhysicalOperator> child,
        std::optional<std::size_t> limit,
        std::optional<std::size_t> offset
    ) noexcept;

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

} // namespace litedb::core::physical_planner::op
