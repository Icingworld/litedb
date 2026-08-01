#pragma once

#include <memory>

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/logical_planner/node/logical_unary_node.hpp"

namespace litedb::core::planner::logical
{

/**
 * @brief 逻辑过滤节点
 */
class LogicalFilter final : public LogicalUnaryNode
{
public:
    LogicalFilter(
        std::unique_ptr<LogicalPlanNode> child,
        std::unique_ptr<binder::bound::BoundExpression> predicate,
        parser::ast::AstNodeLocation location
    );

public:
    /**
     * @brief 获取谓词
     * @return 谓词
     */
    [[nodiscard]]
    const binder::bound::BoundExpression & predicate() const noexcept;

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
    std::unique_ptr<binder::bound::BoundExpression> predicate_;   ///< 谓词
};

} // namespace litedb::core::planner::logical
