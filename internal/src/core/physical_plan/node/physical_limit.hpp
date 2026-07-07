#pragma once

#include <cstddef>
#include <memory>
#include <optional>

#include "core/physical_plan/node/physical_unary_node.hpp"

namespace litedb::core::physical_plan
{

class PhysicalLimit final : public PhysicalUnaryNode
{
public:
    PhysicalLimit(
        std::unique_ptr<PhysicalPlanNode> child,
        std::optional<std::size_t> limit,
        std::optional<std::size_t> offset,
        parser::ast::AstNodeLocation location
    );

    [[nodiscard]]
    std::optional<std::size_t> limit() const noexcept;

    [[nodiscard]]
    std::optional<std::size_t> offset() const noexcept;

    [[nodiscard]]
    std::unique_ptr<PhysicalPlanNode> clone() const override;

private:
    std::optional<std::size_t> limit_;
    std::optional<std::size_t> offset_;
};

} // namespace litedb::core::physical_plan
