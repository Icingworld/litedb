#pragma once

#include <memory>

#include "core/parser/ast/ast_node.hpp"
#include "core/logical_plan/node/logical_plan_node_visitor.hpp"

namespace litedb::core::planner::logical
{

/**
 * @brief 逻辑计划节点类型
 */
enum class LogicalPlanNodeKind
{
    Scan,               ///< 扫描
    VectorSearch,       ///< 向量索引候选搜索
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

    /**
     * @brief 接受访问器
     * @param visitor 访问器
     */
    virtual void accept(LogicalPlanNodeVisitor & visitor) const = 0;

    /**
     * @brief 深拷贝逻辑计划节点
     * @return 逻辑计划节点副本
     */
    [[nodiscard]]
    virtual std::unique_ptr<LogicalPlanNode> clone() const = 0;

private:
    LogicalPlanNodeKind kind_;                  ///< 节点类型
    parser::ast::AstNodeLocation location_;     ///< 节点位置
};

} // namespace litedb::core::planner::logical
