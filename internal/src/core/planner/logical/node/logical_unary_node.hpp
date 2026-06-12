#pragma once

#include <memory>

#include "core/planner/logical/node/logical_plan_node.hpp"

namespace litedb::core::planner::logical
{

/**
 * @brief 逻辑一元节点
 * @details 只有一个子节点的节点
 */
class LogicalUnaryNode : public LogicalPlanNode
{
protected:
    LogicalUnaryNode(
        LogicalPlanNodeKind kind,
        std::unique_ptr<LogicalPlanNode> child,
        parser::ast::AstNodeLocation location
    ) noexcept;

public:
    /**
     * @brief 获取子节点
     * @return 子节点
     */
    [[nodiscard]]
    const LogicalPlanNode & child() const noexcept;

private:
    std::unique_ptr<LogicalPlanNode> child_;   ///< 子节点
};

} // namespace litedb::core::planner::logical
