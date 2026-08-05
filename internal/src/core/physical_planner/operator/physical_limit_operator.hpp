#pragma once

#include <cstddef>
#include <memory>
#include <optional>

#include "core/physical_planner/operator/physical_unary_operator.hpp"

namespace litedb::core::physical_planner::op
{

/**
 * @brief 限制算子
 */
class LimitOperator final : public PhysicalUnaryOperator
{
public:
    LimitOperator(
        std::unique_ptr<PhysicalOperator> child,
        std::optional<std::size_t> limit,
        std::optional<std::size_t> offset
    ) noexcept;

public:
    /**
     * @brief 获取限制
     * @return 限制
     */
    [[nodiscard]]
    std::optional<std::size_t> limit() const noexcept;

    /**
     * @brief 获取偏移
     * @return 偏移
     */
    [[nodiscard]]
    std::optional<std::size_t> offset() const noexcept;

private:
    std::optional<std::size_t> limit_;               ///< 限制
    std::optional<std::size_t> offset_;              ///< 偏移
};

} // namespace litedb::core::physical_planner::op
