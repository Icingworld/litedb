#pragma once

#include <cstddef>
#include <memory>
#include <optional>

#include "core/logical_planner/node/logical_unary_node.hpp"

namespace litedb::core::planner::logical
{

/**
 * @brief 逻辑限制节点
 */
class LogicalLimit final : public LogicalUnaryNode
{
public:
    LogicalLimit(
        std::unique_ptr<LogicalPlanNode> child,
        std::optional<std::size_t> limit,
        std::optional<std::size_t> offset,
        parser::ast::AstNodeLocation location
    );

public:
    /**
     * @brief 获取限制
     * @return 限制
     */
    [[nodiscard]]
    std::optional<std::size_t> limit() const noexcept;

    /**
     * @brief 获取偏移
     * @return 偏移
     */
    [[nodiscard]]
    std::optional<std::size_t> offset() const noexcept;

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
    std::optional<std::size_t> limit_;               ///< 限制
    std::optional<std::size_t> offset_;              ///< 偏移
};

} // namespace litedb::core::planner::logical
