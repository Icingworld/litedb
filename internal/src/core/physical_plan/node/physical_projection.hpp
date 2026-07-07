#pragma once

#include <memory>
#include <vector>

#include "core/binder/bound/statement/bound_select_statement.hpp"
#include "core/physical_plan/node/physical_unary_node.hpp"

namespace litedb::core::physical_plan
{

class PhysicalProjection final : public PhysicalUnaryNode
{
public:
    PhysicalProjection(
        std::unique_ptr<PhysicalPlanNode> child,
        std::vector<binder::bound::BoundProjectionItem> projections,
        parser::ast::AstNodeLocation location
    );

    [[nodiscard]]
    const std::vector<binder::bound::BoundProjectionItem> & projections() const noexcept;

    [[nodiscard]]
    std::unique_ptr<PhysicalPlanNode> clone() const override;

private:
    std::vector<binder::bound::BoundProjectionItem> projections_;
};

} // namespace litedb::core::physical_plan
