#pragma once

#include <memory>
#include <vector>

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/planner/logical/node/logical_unary_node.hpp"

namespace litedb::core::planner::logical
{

/**
 * @brief 逻辑投影节点
 */
class LogicalProjection final : public LogicalUnaryNode
{
public:
    LogicalProjection(
        std::unique_ptr<LogicalPlanNode> child,
        std::vector<std::unique_ptr<binder::bound::BoundExpression>> projections,
        parser::ast::AstNodeLocation location
    );

public:
    /**
     * @brief 获取投影项
     * @return 投影项
     */
    [[nodiscard]]
    const std::vector<std::unique_ptr<binder::bound::BoundExpression>> & projections() const noexcept;

private:
    std::vector<std::unique_ptr<binder::bound::BoundExpression>> projections_;   ///< 投影项
};

} // namespace litedb::core::planner::logical
