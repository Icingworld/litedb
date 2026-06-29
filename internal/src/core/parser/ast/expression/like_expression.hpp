#pragma once

#include <memory>

#include "core/parser/ast/expression/expression_node.hpp"

namespace litedb::core::parser::ast
{

/**
 * @brief LIKE 表达式节点
 * @details 示例：expression LIKE pattern
 */
class LikeExpression final : public ExpressionNode
{
public:
    LikeExpression(
        std::unique_ptr<ExpressionNode> expression,
        std::unique_ptr<ExpressionNode> pattern,
        AstNodeLocation location
    ) noexcept;

public:
    /**
     * @brief 获取节点类型
     * @return 节点类型
     */
    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    /**
     * @brief 接受访问器访问
     * @param visitor 访问器
     */
    void accept(AstNodeVisitor & visitor) const override;

    /**
     * @brief 获取表达式
     * @return 表达式
     */
    [[nodiscard]]
    const ExpressionNode & expression() const noexcept;

    /**
     * @brief 获取模式
     * @return 模式
     */
    [[nodiscard]]
    const ExpressionNode & pattern() const noexcept;

private:
    std::unique_ptr<ExpressionNode> expression_;    ///< 表达式
    std::unique_ptr<ExpressionNode> pattern_;       ///< 模式
};

} // namespace litedb::core::parser::ast
