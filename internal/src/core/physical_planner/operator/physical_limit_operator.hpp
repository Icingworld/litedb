#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <utility>

#include "core/physical_planner/operator/physical_unary_operator.hpp"

namespace litedb::core::physical_planner::op
{

class LimitOperator final : public PhysicalUnaryOperator
{
public:
    LimitOperator(
        std::unique_ptr<PhysicalOperator> child,
        std::optional<std::size_t> limit,
        std::optional<std::size_t> offset
    ) noexcept
        : PhysicalUnaryOperator(PhysicalOperatorKind::Limit, std::move(child))
        , limit_(limit)
        , offset_(offset)
    {
    }

    [[nodiscard]] std::optional<std::size_t> limit() const noexcept
    {
        return limit_;
    }

    [[nodiscard]] std::optional<std::size_t> offset() const noexcept
    {
        return offset_;
    }

private:
    std::optional<std::size_t> limit_;
    std::optional<std::size_t> offset_;
};

} // namespace litedb::core::physical_planner::op
