#pragma once

#include <memory>

#include "core/binder/bound/statement/bound_delete_statement.hpp"
#include "core/binder/bound/statement/bound_select_statement.hpp"
#include "core/binder/bound/statement/bound_update_statement.hpp"
#include "core/planner/logical/node/logical_plan_node.hpp"

namespace litedb::core::planner::logical
{

/**
 * @brief 逻辑计划器
 */
class LogicalPlanner
{
public:
    /**
     * @brief 计划 SELECT 语句
     * @param statement SELECT 语句
     * @return 逻辑计划节点
     * @warning 该成员函数将会移动消费 statement 的成员变量，需要注意 statement 的生命周期
     */
    [[nodiscard]]
    std::unique_ptr<LogicalPlanNode> plan_select(binder::bound::BoundSelectStatement & statement) const;

    /**
     * @brief 计划 UPDATE 输入语句
     * @param statement UPDATE 输入语句
     * @return 逻辑计划节点
     * @details 针对 UPDATE 语句的查询部分
     */
    [[nodiscard]]
    std::unique_ptr<LogicalPlanNode> plan_update_input(binder::bound::BoundUpdateStatement & statement) const;

    /**
     * @brief 计划 DELETE 输入语句
     * @param statement DELETE 输入语句
     * @return 逻辑计划节点
     * @details 针对 DELETE 语句的查询部分
     */
    [[nodiscard]]
    std::unique_ptr<LogicalPlanNode> plan_delete_input(binder::bound::BoundDeleteStatement & statement) const;
};

} // namespace litedb::core::planner::logical
