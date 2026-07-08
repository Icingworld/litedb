#pragma once

#include <memory>

#include "core/logical_plan/node/logical_plan_node.hpp"
#include "core/logical_plan/statement/logical_statement_plan.hpp"

namespace litedb::core::planner::plan
{

/**
 * @brief QUERY 语句计划
 */
class QueryPlan final : public LogicalStatementPlan
{
public:
    QueryPlan(std::unique_ptr<logical::LogicalPlanNode> root, parser::ast::AstNodeLocation location);

public:
    /**
     * @brief 获取根节点
     * @return 根节点
     */
    [[nodiscard]]
    const logical::LogicalPlanNode & root() const noexcept;

private:
    std::unique_ptr<logical::LogicalPlanNode> root_;             ///< 根节点
};

} // namespace litedb::core::planner::plan
