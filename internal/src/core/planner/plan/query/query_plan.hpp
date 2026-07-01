#pragma once

#include <memory>

#include "core/planner/logical/node/logical_plan_node.hpp"
#include "core/planner/plan/statement_plan.hpp"

namespace litedb::core::planner::plan
{

/**
 * @brief QUERY ????
 */
class QueryPlan final : public StatementPlan
{
public:
    QueryPlan(std::unique_ptr<logical::LogicalPlanNode> root, parser::ast::AstNodeLocation location);

public:
    /**
     * @brief ?????
     * @return ???
     */
    [[nodiscard]]
    const logical::LogicalPlanNode & root() const noexcept;

private:
    std::unique_ptr<logical::LogicalPlanNode> root_;             ///< ???
};

} // namespace litedb::core::planner::plan
