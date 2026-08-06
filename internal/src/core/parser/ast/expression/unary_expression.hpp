#pragma once

#include <memory>

#include "core/parser/ast/expression/expression_node.hpp"
#include "core/parser/token.hpp"

namespace litedb::core::parser::ast
{

/**
 * @brief 一元表达式节点
 */
class UnaryExpression final : public ExpressionNode
{
public:
    UnaryExpression(
        TokenType op,
        std::unique_ptr<ExpressionNode> operand,
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
     * @brief 获取操作符
     * @return 操作符
     */
    [[nodiscard]]
    TokenType op() const noexcept;

    /**
     * @brief 获取操作数
     * @return 操作数
     */
    [[nodiscard]]
    const ExpressionNode & operand() const noexcept;

private:
    TokenType op_;                                ///< 操作符
    std::unique_ptr<ExpressionNode> operand_;     ///< 操作数
};

} // namespace litedb::core::parser::ast
