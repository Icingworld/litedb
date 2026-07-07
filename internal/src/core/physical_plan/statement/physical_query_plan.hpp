#pragma once

#include <memory>

#include "core/physical_plan/node/physical_plan_node.hpp"
#include "core/physical_plan/statement/physical_statement_plan.hpp"

namespace litedb::core::physical_plan
{

class PhysicalQueryPlan final : public PhysicalStatementPlan
{
public:
    PhysicalQueryPlan(std::unique_ptr<PhysicalPlanNode> root, parser::ast::AstNodeLocation location);

public:
    [[nodiscard]]
    const PhysicalPlanNode & root() const noexcept;

private:
    std::unique_ptr<PhysicalPlanNode> root_;
};

} // namespace litedb::core::physical_plan
