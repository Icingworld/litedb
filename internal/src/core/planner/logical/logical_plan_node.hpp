#pragma once

#include "core/parser/ast/ast_node.hpp"

namespace litedb::core::planner::logical
{

/**
 * @brief 逻辑计划节点类型
 */
enum class LogicalPlanNodeKind
{
    Scan,               ///< 扫描
    Filter,             ///< 过滤
    Projection,         ///< 投影
    OrderBy,            ///< 排序
    Limit,              ///< 限制
};

/**
 * @brief 逻辑计划节点
 */
class LogicalPlanNode
{
public:
    LogicalPlanNode(const LogicalPlanNode &) = delete;

    LogicalPlanNode & operator=(const LogicalPlanNode &) = delete;

    LogicalPlanNode(LogicalPlanNode &&) noexcept = default;

    LogicalPlanNode & operator=(LogicalPlanNode &&) noexcept = default;

    virtual ~LogicalPlanNode() noexcept = default;

protected:
    LogicalPlanNode(LogicalPlanNodeKind kind, parser::ast::AstNodeLocation location) noexcept;

public:
    /**
     * @brief 获取节点类型
     * @return 节点类型
     */
    [[nodiscard]]
    LogicalPlanNodeKind kind() const noexcept;

    /**
     * @brief 获取节点位置
     * @return 节点位置
     */
    [[nodiscard]]
    parser::ast::AstNodeLocation location() const noexcept;

private:
    LogicalPlanNodeKind kind_;                  ///< 节点类型
    parser::ast::AstNodeLocation location_;     ///< 节点位置
};

} // namespace litedb::core::planner::logical
