#pragma once

#include <memory>

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/physical_plan/node/physical_unary_node.hpp"

namespace litedb::core::physical_plan
{

class PhysicalFilter final : public PhysicalUnaryNode
{
public:
    PhysicalFilter(
        std::unique_ptr<PhysicalPlanNode> child,
        std::unique_ptr<binder::bound::BoundExpression> predicate,
        parser::ast::AstNodeLocation location
    );

    [[nodiscard]]
    const binder::bound::BoundExpression & predicate() const noexcept;

    [[nodiscard]]
    std::unique_ptr<PhysicalPlanNode> clone() const override;

private:
    std::unique_ptr<binder::bound::BoundExpression> predicate_;
};

} // namespace litedb::core::physical_plan
