#pragma once

#include <memory>
#include <vector>

#include "core/binder/bound/statement/bound_select_statement.hpp"
#include "core/physical_planner/node/physical_unary_node.hpp"

namespace litedb::core::physical_plan
{

class PhysicalSort final : public PhysicalUnaryNode
{
public:
    PhysicalSort(
        std::unique_ptr<PhysicalPlanNode> child,
        std::vector<binder::bound::BoundOrderByItem> order_by,
        parser::ast::AstNodeLocation location
    );

    [[nodiscard]]
    const std::vector<binder::bound::BoundOrderByItem> & order_by() const noexcept;

    [[nodiscard]]
    std::unique_ptr<PhysicalPlanNode> clone() const override;

private:
    std::vector<binder::bound::BoundOrderByItem> order_by_;
};

} // namespace litedb::core::physical_plan
