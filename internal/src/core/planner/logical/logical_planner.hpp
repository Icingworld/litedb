#pragma once

#include <memory>

#include "core/index/index_manager.hpp"
#include "core/binder/bound/statement/bound_delete_statement.hpp"
#include "core/binder/bound/statement/bound_select_statement.hpp"
#include "core/binder/bound/statement/bound_update_statement.hpp"
#include "core/planner/access_path/access_path_selector.hpp"
#include "core/planner/logical/node/logical_plan_node.hpp"

namespace litedb::core::planner::logical
{

/**
 * @brief 逻辑计划器
 */
class LogicalPlanner
{
public:
    LogicalPlanner() noexcept;

    explicit LogicalPlanner(const index::IndexManager * index_manager) noexcept;

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

private:
    access_path::AccessPathSelector access_path_selector_;  ///< 访问路径选择器
};

} // namespace litedb::core::planner::logical
