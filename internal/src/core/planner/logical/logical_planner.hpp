#pragma once

#include <memory>

#include "core/binder/bound/statement/bound_delete_statement.hpp"
#include "core/binder/bound/statement/bound_select_statement.hpp"
#include "core/binder/bound/statement/bound_update_statement.hpp"
#include "core/planner/logical/logical_plan_node.hpp"

namespace litedb::core::planner::logical
{

/**
 * @brief 逻辑计划器
 */
class LogicalPlanner
{
public:
    /**
     * @brief 计划选择语句
     * @param statement 选择语句
     * @return 逻辑计划节点
     */
    [[nodiscard]]
    std::unique_ptr<LogicalPlanNode> plan_select(binder::bound::BoundSelectStatement & statement) const;

    /**
     * @brief 计划更新输入
     * @param statement 更新语句
     * @return 逻辑计划节点
     */
    [[nodiscard]]
    std::unique_ptr<LogicalPlanNode> plan_update_input(binder::bound::BoundUpdateStatement & statement) const;

    /**
     * @brief 计划删除输入
     * @param statement 删除语句
     * @return 逻辑计划节点
     */
    [[nodiscard]]
    std::unique_ptr<LogicalPlanNode> plan_delete_input(binder::bound::BoundDeleteStatement & statement) const;
};

} // namespace litedb::core::planner::logical
