#pragma once

#include <memory>

#include "core/parser/ast/expression/expression_node.hpp"

namespace litedb::core::parser::ast
{

/**
 * @brief BETWEEN 表达式节点
 * @details 示例：expression BETWEEN lower AND upper
 */
class BetweenExpression final : public ExpressionNode
{
public:
    BetweenExpression(
        std::unique_ptr<ExpressionNode> expression,
        std::unique_ptr<ExpressionNode> lower,
        std::unique_ptr<ExpressionNode> upper,
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
     * @brief 获取下界
     * @return 下界
     */
    [[nodiscard]]
    const ExpressionNode & lower() const noexcept;

    /**
     * @brief 获取上界
     * @return 上界
     */
    [[nodiscard]]
    const ExpressionNode & upper() const noexcept;

private:
    std::unique_ptr<ExpressionNode> expression_; ///< 表达式
    std::unique_ptr<ExpressionNode> lower_;      ///< 下界
    std::unique_ptr<ExpressionNode> upper_;      ///< 上界
};

} // namespace litedb::core::parser::ast
