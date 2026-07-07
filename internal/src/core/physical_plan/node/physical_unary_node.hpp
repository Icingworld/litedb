#pragma once

#include <memory>

#include "core/physical_plan/node/physical_plan_node.hpp"

namespace litedb::core::physical_plan
{

class PhysicalUnaryNode : public PhysicalPlanNode
{
protected:
    PhysicalUnaryNode(
        PhysicalPlanNodeKind kind,
        std::unique_ptr<PhysicalPlanNode> child,
        parser::ast::AstNodeLocation location
    ) noexcept;

public:
    [[nodiscard]]
    const PhysicalPlanNode & child() const noexcept;

private:
    std::unique_ptr<PhysicalPlanNode> child_;
};

} // namespace litedb::core::physical_plan
