#pragma once

#include <memory>
#include <vector>

#include "core/binder/bound/statement/bound_select_statement.hpp"
#include "core/logical_plan/node/logical_unary_node.hpp"

namespace litedb::core::planner::logical
{

/**
 * @brief 逻辑排序节点
 */
class LogicalOrderBy final : public LogicalUnaryNode
{
public:
    LogicalOrderBy(
        std::unique_ptr<LogicalPlanNode> child,
        std::vector<binder::bound::BoundOrderByItem> order_by,
        parser::ast::AstNodeLocation location
    );

public:
    /**
     * @brief 获取排序项
     * @return 排序项
     */
    [[nodiscard]]
    const std::vector<binder::bound::BoundOrderByItem> & order_by() const noexcept;

    /**
     * @brief 接受访问器
     * @param visitor 访问器
     */
    void accept(LogicalPlanNodeVisitor & visitor) const override;

    /**
     * @brief 深拷贝逻辑计划节点
     * @return 逻辑计划节点副本
     */
    [[nodiscard]]
    std::unique_ptr<LogicalPlanNode> clone() const override;

private:
    std::vector<binder::bound::BoundOrderByItem> order_by_;   ///< 排序项
};

} // namespace litedb::core::planner::logical
